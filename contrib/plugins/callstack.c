/*
 * Callstack Plugin for AArch64
 *
 * Copyright (C) 2025 
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu/plugin.h"
#include <inttypes.h>
#include <assert.h>
#include <glib.h>
#include <fcntl.h>
#include "elf.h" 

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define MAX_CALLSTACK_DEPTH 64
#define MAX_SYMBOL_LENGTH 64

typedef struct {
    uint64_t addr;
    const char *symbol;
} CallStackEntry;

typedef struct {
    CallStackEntry *entries;
    int depth;
} ThreadCallStack;

typedef struct {
    GHashTable *thread_stacks;
    GMutex lock;
} ProcessCallStacks;

/* cached per-vcpu state */
typedef struct {
    uint64_t ttbr0;         /* Cached TTBR value */
    uint64_t ttbr1;         /* Cached TTBR value */
    uint64_t tcr;
    bool ttbr_dirty;       /* Flag indicating if cache needs refresh */
    uint64_t taskIdCurrent;
    GMutex lock;          /* Lock for this cache entry */
} VCPUCache;

/* Struct to hold parsed instruction info */
typedef struct {
    bool is_call;
    bool is_ret;
    char reg_name[4];  // For storing register name from blr
    uint64_t target;   // For storing immediate from bl
    uint64_t insn_addr;  // Original instruction address for fallback
    uint64_t sp;       // Stack pointer
} InsnInfo;

/* Global state */
static const char *file_name;
static bool stack_heuristic = false;
static bool stack_vxworks = false;
static bool split_ttbr0_ttbr1 = false;

static GHashTable *process_stacks;
static GMutex stacks_lock;
static GArray *vcpu_caches;
static GMutex vcpu_caches_lock;

static GHashTable *kernel_addr_to_sym;
uint64_t vxKernelVarsAddr = (uint64_t)-1;

static VCPUCache *get_vcpu_cache(unsigned int cpu_index) 
{
    VCPUCache *cache;
    
    g_mutex_lock(&vcpu_caches_lock);
    cache = &g_array_index(vcpu_caches, VCPUCache, cpu_index);
    g_mutex_unlock(&vcpu_caches_lock);
    
    return cache;
}

#define STACK_SIZE_MAX 0x1000
static ThreadCallStack *get_thread_stack(ProcessCallStacks *pstacks, uint64_t tid)
{
    ThreadCallStack *tstack = NULL;
    GHashTableIter iter;
    gpointer key, value;

    if (!pstacks) {
        return NULL;
    }

    if (!pstacks->thread_stacks) {
        return NULL;
    }

    g_mutex_lock(&pstacks->lock);

    g_hash_table_iter_init(&iter, pstacks->thread_stacks);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        uint64_t seen_tid = (uint64_t)key;
        uint64_t distance;
        ThreadCallStack *seen_tstack = (ThreadCallStack *)value;

        if (stack_heuristic) {
            /* if using stack segregation heuristic, find the nearest
             * previously extrapolated stack base to group this func
             * call/return with
             */
            if (tid < seen_tid) {
                /* stack grows down */
                distance = seen_tid - tid;
                if (distance < STACK_SIZE_MAX) {
                    tstack = seen_tstack;
                    break;
                }
            }
        } else if (stack_vxworks) {
            if (tid == seen_tid) {
                tstack = seen_tstack;
            }
        } else {
            /* if not using stack segregation heuristic, there is only
             * one tstack, so break at the first loop iteration
             */
            tstack = seen_tstack;
            break;
        }
    }

    if (!tstack) {
        tstack = g_new0(ThreadCallStack, 1);
        if (tstack) {
            tstack->entries = g_malloc0(sizeof(CallStackEntry) * MAX_CALLSTACK_DEPTH);
            if (!tstack->entries) {
                g_free(tstack);
                tstack = NULL;
            } else {
                if (stack_heuristic) {
                    /* stack grows down, align up */
                    g_hash_table_insert(pstacks->thread_stacks, GUINT_TO_POINTER(((tid + STACK_SIZE_MAX - 1) & ~(STACK_SIZE_MAX - 1))), tstack);
                } else if (stack_vxworks) {
                    g_hash_table_insert(pstacks->thread_stacks, GUINT_TO_POINTER(tid), tstack);
                } else {
                    g_hash_table_insert(pstacks->thread_stacks, GUINT_TO_POINTER(0), tstack);
                }
            }
        }
    }

    g_mutex_unlock(&pstacks->lock);

    return tstack;
}

static ProcessCallStacks *get_process(uint64_t ttbr)
{
    ProcessCallStacks *stack;
    
    if (!process_stacks) {
        return NULL;
    }
    
    g_mutex_lock(&stacks_lock);
    stack = g_hash_table_lookup(process_stacks, GUINT_TO_POINTER(ttbr));
    if (!stack) {
        stack = g_new0(ProcessCallStacks, 1);
        if (stack) {
            stack->thread_stacks = g_hash_table_new_full(NULL, g_direct_equal, NULL, g_free);
            if (!stack->thread_stacks) {
                g_free(stack);
                stack = NULL;
            } else {
                g_mutex_init(&stack->lock);
                g_hash_table_insert(process_stacks, GUINT_TO_POINTER(ttbr), stack);
            }
        }
    }
    g_mutex_unlock(&stacks_lock);
    
    return stack;
}

static void read_reg(qemu_plugin_reg_descriptor *desc, uint64_t *dest)
{
    GByteArray *reg_buf = g_byte_array_new();
    int regsize = qemu_plugin_read_register(desc->handle, reg_buf);

    if (regsize > 0) {
        for (int j = regsize-1; j >= 0; j--) {
            *dest = (*dest << 8) | reg_buf->data[j];
        }
    }
    g_byte_array_free(reg_buf, TRUE);
}

static void read_current_ttbr(VCPUCache *cache)
{
    // Read both TTBR0 and TTBR1
    g_autoptr(GString) ttbr0_reg_prefix = g_string_new("TTBR0_EL1");
    g_autoptr(GString) ttbr1_reg_prefix = g_string_new("TTBR1_EL1");
    // Also read TCR_EL1 to determine which addresses to associate with TTBR0 or TTBR1
    g_autoptr(GString) tcr_reg_prefix = g_string_new("TCR_EL1");
    GArray *reg_list = qemu_plugin_get_registers();
    bool ttbr0_seen = false;
    bool ttbr1_seen = false;
    bool tcr_seen = false;

    /* reset cached values */
    cache->ttbr0 = 0;
    cache->ttbr1 = 0;
    cache->tcr = 0;

    if (!reg_list) {
        return;
    }

    for (int i = 0; i < reg_list->len; i++) {
        qemu_plugin_reg_descriptor *desc = &g_array_index(reg_list, 
            qemu_plugin_reg_descriptor, i);
        if (strncmp(desc->name, ttbr0_reg_prefix->str, ttbr0_reg_prefix->len) == 0) {
            read_reg(desc, &cache->ttbr0);
            ttbr0_seen =  true;
        }
        if (strncmp(desc->name, ttbr1_reg_prefix->str, ttbr1_reg_prefix->len) == 0) {
            read_reg(desc, &cache->ttbr1);
            ttbr1_seen =  true;
        }
        if (strncmp(desc->name, tcr_reg_prefix->str, tcr_reg_prefix->len) == 0) {
            read_reg(desc, &cache->tcr);
            tcr_seen = true;
        }
        if (ttbr0_seen && ttbr1_seen && tcr_seen) {
            break;
        }
    }
    g_array_free(reg_list, TRUE);
    return;
}

static uint64_t read_gp_register(const char *reg_name)
{
    GArray *reg_list = qemu_plugin_get_registers();
    uint64_t value = 0;
    
    if (!reg_list) {
        return 0;
    }

    for (int i = 0; i < reg_list->len; i++) {
        qemu_plugin_reg_descriptor *desc = &g_array_index(reg_list, 
            qemu_plugin_reg_descriptor, i);
        if (g_ascii_strcasecmp(desc->name, reg_name) == 0) {
            read_reg(desc, &value);
            break;
        }
    }
    g_array_free(reg_list, TRUE);
    return value;
}

static void vcpu_ttbr_exec(unsigned int cpu_index, void *udata)
{
    VCPUCache *cache = get_vcpu_cache(cpu_index);
    if (!cache) {
        return;
    }
    
    g_mutex_lock(&cache->lock);
    cache->ttbr_dirty = true;  /* Mark cache as needing refresh */
    g_mutex_unlock(&cache->lock);
}

static void update_cached_ttbr(VCPUCache *cache)
{
    g_assert_nonnull(cache);
    
    g_mutex_lock(&cache->lock);
    if (cache->ttbr_dirty) {
        read_current_ttbr(cache);
        cache->ttbr_dirty = false;
    }
    g_mutex_unlock(&cache->lock);
}

static void vcpu_insn_exec(unsigned int cpu_index, void *udata)
{
    InsnInfo *info = (InsnInfo *)udata;
    ProcessCallStacks *pstacks;
    ThreadCallStack *tstack;
    const char *sym;
    uint64_t target_addr;
    uint64_t tcr_t0sz;
    uint64_t mask;
    VCPUCache *cache;

    if (!info) {
        return;
    }

    if (!info->is_call && !info->is_ret) {
        return;
    }

    cache = get_vcpu_cache(cpu_index);

    update_cached_ttbr(cache);

    if (split_ttbr0_ttbr1) {
        // get number of bits used for VA space addressed through TTBR0
        tcr_t0sz = cache->tcr & 0x3F;
        mask = ~((1ULL << (64 - tcr_t0sz)) - 1);
        pstacks = ((info->insn_addr & mask) == 0) ? get_process(cache->ttbr0) : get_process(cache->ttbr1);
    } else {
        pstacks = get_process(cache->ttbr0);
    }
    
    if (stack_vxworks) {
        tstack = get_thread_stack(pstacks, cache->taskIdCurrent);
    } else {
        tstack = get_thread_stack(pstacks, info->sp);
    }


    if (!tstack) {
        return;
    }

    g_mutex_lock(&pstacks->lock);

    if (info->is_call) {
        if (tstack->depth < MAX_CALLSTACK_DEPTH - 1) {
            if (info->reg_name[0]) { // This is a blr instruction
                target_addr = read_gp_register(info->reg_name);
            } else { // This is a bl instruction
                target_addr = info->target;
            }

            /* Look up symbol in kernel symbol table */
            sym = "<unknown>";
            if (kernel_addr_to_sym) {
                const char *kernel_sym = g_hash_table_lookup(kernel_addr_to_sym, 
                                                          GUINT_TO_POINTER(target_addr));
                if (kernel_sym) {
                    sym = kernel_sym;
                }
            }
            
            tstack->entries[tstack->depth].addr = target_addr;
            tstack->entries[tstack->depth].symbol = sym;
            tstack->depth++;
        }
    } else if (info->is_ret && tstack->depth > 0) {
        tstack->depth--;
    }

    g_mutex_unlock(&pstacks->lock);
}

static void windvars_update_cb(unsigned int cpu_index, qemu_plugin_meminfo_t info,
                               uint64_t vaddr, void *udata)
{
    VCPUCache *cache;

    cache = get_vcpu_cache(cpu_index);

    /*
     * vxKernelVars[] is an array of WIND_VARS structs indexed by CPU ID.
     * Each entry of vxKernelVars is 256 bytes. This is calculated based
     * on the 152 byte size of _windVars (as of vxWorks 25.03) aligned to
     * 128 bytes.
     */
#define SIZE_WIND_VARS 256
    if (vaddr == (vxKernelVarsAddr + (cpu_index * SIZE_WIND_VARS)) &&
        qemu_plugin_mem_is_store(info)) {
        qemu_plugin_mem_value val;

        g_mutex_lock(&cache->lock);
        val = qemu_plugin_mem_get_value(info);
        cache->taskIdCurrent = val.data.u64;
        g_mutex_unlock(&cache->lock);
    }

    return;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    if (!tb) {
        return;
    }
    
    size_t n = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        if (!insn) {
            continue;
        }
        
        char *disas = qemu_plugin_insn_disas(insn);
        if (!disas) {
            continue;
        }

        /* Register TTBR exec callback for MSR instructions that might modify TTBR */
        if (g_str_has_prefix(disas, "msr ") && strstr(disas, "ttbr")) {
            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_ttbr_exec,
                                                 QEMU_PLUGIN_CB_R_REGS, NULL);
        }

        /* Handle branch instructions */
        if (g_str_has_prefix(disas, "bl ") || g_str_has_prefix(disas, "blr ") || 
            g_str_has_prefix(disas, "ret")) {
            InsnInfo *info = g_new0(InsnInfo, 1);
            info->insn_addr = qemu_plugin_insn_vaddr(insn);
            info->sp = read_gp_register("sp");
            
            if (g_str_has_prefix(disas, "bl ")) {
                info->is_call = true;
                /* Parse immediate from disassembly */
                char *offset_str = strchr(disas, '#');
                if (offset_str) {
                    info->target = strtoull(offset_str + 1, NULL, 16);
                } else {
                    info->target = info->insn_addr; /* fallback */
                }
            } else if (g_str_has_prefix(disas, "blr ")) {
                info->is_call = true;
                /* Parse register name */
                char *reg_str = disas + 4; /* Skip "blr " */
                while (*reg_str == ' ') reg_str++; /* Skip spaces */
                strncpy(info->reg_name, reg_str, sizeof(info->reg_name) - 1);
                info->reg_name[sizeof(info->reg_name) - 1] = '\0';
            } else { // ret instruction
                info->is_ret = true;
            }

            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                                 QEMU_PLUGIN_CB_R_REGS,
                                                 info);
        }

        /* check for updates to taskIdCurrent */
        qemu_plugin_register_vcpu_mem_cb(insn, windvars_update_cb,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         QEMU_PLUGIN_MEM_W, NULL);

        g_free(disas);
    }
}

/* Parse ELF file and populate kernel symbol table */
static bool parse_kernel_symbols(void) 
{
    int fd;
    Elf64_Ehdr ehdr;
    Elf64_Shdr *shdr = NULL;
    char *strtab = NULL;
    Elf64_Sym *syms = NULL;
    int sym_idx = 0, str_idx = 0;
    int i, nsyms;
    bool success = false;

    if (!file_name) {
        fprintf(stderr, "No kernel ELF file specified\n");
        return false;
    }

    kernel_addr_to_sym = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                              NULL, g_free);
    if (!kernel_addr_to_sym) {
        fprintf(stderr, "Failed to create symbol hash table\n");
        return false;
    }

    fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file %s: %s\n", file_name, strerror(errno));
        goto cleanup;
    }

    /* Read ELF header */
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        fprintf(stderr, "Failed to read ELF header: %s\n", strerror(errno));
        goto cleanup;
    }

    /* Check ELF magic */
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3) {
        fprintf(stderr, "Not a valid ELF file (wrong magic)\n");
        goto cleanup;
    }

    /* Verify 64-bit ELF */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Not a 64-bit ELF file\n");
        goto cleanup;
    }

    /* Read section headers */
    shdr = g_malloc0(sizeof(Elf64_Shdr) * ehdr.e_shnum);
    if (!shdr) {
        fprintf(stderr, "Failed to allocate section headers\n");
        goto cleanup;
    }

    if (lseek(fd, ehdr.e_shoff, SEEK_SET) < 0) {
        fprintf(stderr, "Failed to seek to section headers: %s\n", strerror(errno));
        goto cleanup;
    }

    ssize_t bytes_read = read(fd, shdr, sizeof(Elf64_Shdr) * ehdr.e_shnum);
    if (bytes_read != sizeof(Elf64_Shdr) * ehdr.e_shnum) {
        fprintf(stderr, "Failed to read section headers (read %zd bytes, expected %zu): %s\n",
                bytes_read, sizeof(Elf64_Shdr) * ehdr.e_shnum, strerror(errno));
        goto cleanup;
    }

    /* Find symbol table and string table sections */
    for (i = 0; i < ehdr.e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            sym_idx = i;
            str_idx = shdr[i].sh_link;
            break;
        }
    }

    if (!sym_idx) {
        fprintf(stderr, "No symbol table found in ELF file\n");
        goto cleanup;
    }

    if (str_idx >= ehdr.e_shnum) {
        fprintf(stderr, "Invalid string table index %d\n", str_idx);
        goto cleanup;
    }

    /* Read string table */
    strtab = g_malloc0(shdr[str_idx].sh_size);
    if (!strtab) {
        fprintf(stderr, "Failed to allocate string table\n");
        goto cleanup;
    }

    if (lseek(fd, shdr[str_idx].sh_offset, SEEK_SET) < 0) {
        fprintf(stderr, "Failed to seek to string table: %s\n", strerror(errno));
        goto cleanup;
    }

    bytes_read = read(fd, strtab, shdr[str_idx].sh_size);
    if (bytes_read != shdr[str_idx].sh_size) {
        fprintf(stderr, "Failed to read string table (read %zd bytes, expected %zu): %s\n",
                bytes_read, (size_t)shdr[str_idx].sh_size, strerror(errno));
        goto cleanup;
    }

    /* Read symbol table */
    syms = g_malloc0(shdr[sym_idx].sh_size);
    if (!syms) {
        fprintf(stderr, "Failed to allocate symbol table\n");
        goto cleanup;
    }

    if (lseek(fd, shdr[sym_idx].sh_offset, SEEK_SET) < 0) {
        fprintf(stderr, "Failed to seek to symbol table: %s\n", strerror(errno));
        goto cleanup;
    }

    bytes_read = read(fd, syms, shdr[sym_idx].sh_size);
    if (bytes_read != shdr[sym_idx].sh_size) {
        fprintf(stderr, "Failed to read symbol table (read %zd bytes, expected %zu): %s\n",
                bytes_read, (size_t)shdr[sym_idx].sh_size, strerror(errno));
        goto cleanup;
    }

    nsyms = shdr[sym_idx].sh_size / sizeof(Elf64_Sym);
    fprintf(stderr, "Found %d symbols\n", nsyms);

    int func_count = 0;
    for (i = 0; i < nsyms; i++) {
        if (syms[i].st_name) {
            /* add func symbols to kernel_addr_to_sym map */
            if (ELF64_ST_TYPE(syms[i].st_info) == STT_FUNC) {
                char *name = g_strdup(strtab + syms[i].st_name);
                if (name) {
                    g_hash_table_insert(kernel_addr_to_sym,
                                        GUINT_TO_POINTER(syms[i].st_value),
                                        name);
                }
                func_count++;
            }
            /* get address of vxKernelVars from symbol table */
            if (ELF64_ST_TYPE(syms[i].st_info) == STT_OBJECT &&
                g_strcmp0(strtab + syms[i].st_name, "vxKernelVars") == 0) {
                vxKernelVarsAddr = syms[i].st_value;
            }
        }
    }
    fprintf(stderr, "Added %d function symbols to hash table\n", func_count);

    success = true;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    g_free(shdr);
    g_free(strtab); 
    g_free(syms);

    if (!success) {
        if (kernel_addr_to_sym) {
            g_hash_table_destroy(kernel_addr_to_sym);
            kernel_addr_to_sym = NULL;
        }
    }

    return success;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_autoptr(GString) report = g_string_new("Callstack Report By TTBR:\n");
    GHashTableIter iter;
    gpointer key, value;
    int i;
    
    if (!process_stacks || !report) {
        return;
    }
    
    g_mutex_lock(&stacks_lock);
    g_hash_table_iter_init(&iter, process_stacks);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        uint64_t ttbr = (uint64_t)key;
        ProcessCallStacks *pstacks = (ProcessCallStacks *)value;
        GHashTableIter tstack_iter;
        gpointer tstack_key, tstack_value;
        
        if (!pstacks) {
            continue;
        }

        g_mutex_lock(&pstacks->lock);
        
        g_hash_table_iter_init(&tstack_iter, pstacks->thread_stacks);

        while (g_hash_table_iter_next(&tstack_iter, &tstack_key, &tstack_value)) {
            uint64_t sp = (uint64_t)tstack_key;
            ThreadCallStack *tstack = (ThreadCallStack *)tstack_value;
        
            if (stack_vxworks) {
                g_string_append_printf(report, "\nTTBR 0x%" PRIx64 " TCB 0x%" PRIx64 " callstack depth: %d\n",
                                       ttbr, sp, tstack->depth);
            } else {
                g_string_append_printf(report, "\nTTBR 0x%" PRIx64 " SP 0x%" PRIx64 " callstack depth: %d\n",
                                       ttbr, sp, tstack->depth);
            }
            
            if (tstack->depth > 0) {
                g_string_append_printf(report, "Current callstack:\n");
                for (i = 0; i < tstack->depth; i++) {
                    g_string_append_printf(report,
                                      "  #%-2d 0x%" PRIx64 " in %s\n",
                                      i, tstack->entries[i].addr,
                                      tstack->entries[i].symbol);
                }
            }
            g_free(tstack->entries);
        }
        
        g_hash_table_destroy(pstacks->thread_stacks);
        g_mutex_unlock(&pstacks->lock);
        g_mutex_clear(&pstacks->lock);
    }
    
    qemu_plugin_outs(report->str);
    
    /* Clean up vcpu caches */
    g_mutex_lock(&vcpu_caches_lock);
    for (i = 0; i < vcpu_caches->len; i++) {
        VCPUCache *c = &g_array_index(vcpu_caches, VCPUCache, i);
        g_mutex_clear(&c->lock);
    }
    g_array_free(vcpu_caches, true);
    g_mutex_unlock(&vcpu_caches_lock);
    g_mutex_clear(&vcpu_caches_lock);
    
    g_mutex_unlock(&stacks_lock);
    g_hash_table_destroy(process_stacks);
    g_mutex_clear(&stacks_lock);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                         const qemu_info_t *info,
                                         int argc, char **argv)
{
    /* Only support aarch64 targets */
    if (!strstr(info->target_name, "aarch64")) {
        fprintf(stderr, "This plugin only supports aarch64 targets\n");
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_auto(GStrv) tokens = g_strsplit(opt, "=", 2);
        if (g_strcmp0(tokens[0], "kernel_elf") == 0) {
            file_name = g_strdup(tokens[1]);
        }
        if (g_strcmp0(tokens[0], "stack_heuristic") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &stack_heuristic)) {
                fprintf(stderr, "boolean arg parsing failed: %s\n", opt);
                return -1;
            }
        }
        if (g_strcmp0(tokens[0], "stack_vxworks") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &stack_vxworks)) {
                fprintf(stderr, "boolean arg parsing failed: %s\n", opt);
                return -1;
            }
        }
        if (g_strcmp0(tokens[0], "split_ttbr0_ttbr1") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &split_ttbr0_ttbr1)) {
                fprintf(stderr, "boolean arg parsing failed: %s\n", opt);
                return -1;
            }
        }
    }

    /* Initialize symbol table if kernel ELF file was provided */
    if (file_name && !parse_kernel_symbols()) {
        fprintf(stderr, "Failed to parse kernel symbols from %s\n", file_name);
        return -1;
    }

    /* check if vxKernelVars is successfully parsed if using stack_vxworks */
    if (stack_vxworks && vxKernelVarsAddr == (uint64_t)-1) {
        fprintf(stderr, "Failed to parse vxKernelVars from %s\n", file_name);
    }

    vcpu_caches = g_array_sized_new(true, true, sizeof(VCPUCache), info->system.max_vcpus);
    if (!vcpu_caches) {
        return -1;
    }
    for (int i = 0; i < vcpu_caches->len; i++) {
        VCPUCache *c = &g_array_index(vcpu_caches, VCPUCache, i);
        g_mutex_init(&c->lock);
        /* mark dirty to force initial read */
        c->ttbr_dirty = true;
    }
    g_mutex_init(&vcpu_caches_lock);

    process_stacks = g_hash_table_new_full(NULL, g_direct_equal, NULL, g_free);
    if (!process_stacks) {
        g_array_free(vcpu_caches, true);
        g_mutex_clear(&vcpu_caches_lock);
        return -1;
    }
    g_mutex_init(&stacks_lock);
    
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    
    return 0;
}
