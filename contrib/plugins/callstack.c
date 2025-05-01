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
} ProcessCallStack;

/* Per-vCPU TTBR cache */
typedef struct {
    uint64_t ttbr0;         /* Cached TTBR value */
    uint64_t ttbr1;         /* Cached TTBR value */
    bool ttbr_dirty;       /* Flag indicating if cache needs refresh */
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
static GHashTable *process_stacks;
static GMutex stacks_lock;
static GHashTable *vcpu_caches;  /* Map of CPU index to VCPUCache */
static GMutex vcpu_caches_lock;

static VCPUCache *get_vcpu_cache(unsigned int cpu_index) 
{
    VCPUCache *cache;
    
    g_mutex_lock(&vcpu_caches_lock);
    cache = g_hash_table_lookup(vcpu_caches, GUINT_TO_POINTER(cpu_index));
    if (!cache) {
        cache = g_new0(VCPUCache, 1);
        if (cache) {
            g_mutex_init(&cache->lock);
            cache->ttbr_dirty = true; /* Force initial read */
            g_hash_table_insert(vcpu_caches, GUINT_TO_POINTER(cpu_index), cache);
        }
    }
    g_mutex_unlock(&vcpu_caches_lock);
    
    return cache;
}

static ThreadCallStack *get_thread_stack(ProcessCallStack *pstack, uint64_t sp)
{
    ThreadCallStack *tstack;

    if (!pstack) {
        return NULL;
    }

    if (!pstack->thread_stacks) {
        return NULL;
    }

    g_mutex_lock(&pstack->lock);
    tstack = g_hash_table_lookup(pstack->thread_stacks, GUINT_TO_POINTER(sp));
    if (!tstack) {
        tstack = g_new0(ThreadCallStack, 1);
        if (tstack) {
            tstack->entries = g_malloc0(sizeof(CallStackEntry) * MAX_CALLSTACK_DEPTH);
            if (!tstack->entries) {
                g_free(tstack);
                tstack = NULL;
            } else {
                g_hash_table_insert(pstack->thread_stacks, GUINT_TO_POINTER(sp), tstack);
            }
        }
    }
    g_mutex_unlock(&pstack->lock);

    return tstack;
}

static ProcessCallStack *get_process_stack(uint64_t ttbr)
{
    ProcessCallStack *stack;
    
    if (!process_stacks) {
        return NULL;
    }
    
    g_mutex_lock(&stacks_lock);
    stack = g_hash_table_lookup(process_stacks, GUINT_TO_POINTER(ttbr));
    if (!stack) {
        stack = g_new0(ProcessCallStack, 1);
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

static void read_current_ttbr(VCPUCache *cache)
{
    g_autoptr(GString) ttbr0_reg_prefix = g_string_new("TTBR0_EL1");
    g_autoptr(GString) ttbr1_reg_prefix = g_string_new("TTBR1_EL1");
    GArray *reg_list = qemu_plugin_get_registers();
    bool ttbr0_seen = false;
    bool ttbr1_seen = false;

    /* reset cached ttbr values */
    cache->ttbr0 = 0;
    cache->ttbr1 = 0;

    if (!reg_list) {
        return;
    }

    for (int i = 0; i < reg_list->len; i++) {
        qemu_plugin_reg_descriptor *desc = &g_array_index(reg_list, 
            qemu_plugin_reg_descriptor, i);
        if (strncmp(desc->name, ttbr0_reg_prefix->str, ttbr0_reg_prefix->len) == 0) {
            GByteArray *reg_buf = g_byte_array_new();
            int regsize = qemu_plugin_read_register(desc->handle, reg_buf);

            if (regsize > 0) {
                for (int j = regsize-1; j >= 0; j--) {
                    cache->ttbr0 = (cache->ttbr0 << 8) | reg_buf->data[j];
                }
            }
            g_byte_array_free(reg_buf, TRUE);
            ttbr0_seen =  true;
        }
        if (strncmp(desc->name, ttbr1_reg_prefix->str, ttbr1_reg_prefix->len) == 0) {
            GByteArray *reg_buf = g_byte_array_new();
            int regsize = qemu_plugin_read_register(desc->handle, reg_buf);
            
            if (regsize > 0) {
                for (int j = regsize-1; j >= 0; j--) {
                    cache->ttbr1 = (cache->ttbr1 << 8) | reg_buf->data[j];
                }
            }
            g_byte_array_free(reg_buf, TRUE);
            ttbr1_seen =  true;
        }
        if (ttbr0_seen && ttbr1_seen) {
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
            GByteArray *reg_buf = g_byte_array_new();
            int regsize = qemu_plugin_read_register(desc->handle, reg_buf);
            
            if (regsize > 0) {
                for (int j = regsize-1; j >= 0; j--) {
                    value = (value << 8) | reg_buf->data[j];
                }
            }
            g_byte_array_free(reg_buf, TRUE);
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
    ProcessCallStack *pstack;
    ThreadCallStack *tstack;
    const char *sym = "<unknown>";
    uint64_t target_addr;
    VCPUCache *cache;

    if (!info) {
        return;
    }
    
    if (!info->is_call && !info->is_ret) {
        return;
    }

    cache = get_vcpu_cache(cpu_index);

    update_cached_ttbr(cache);

    if ((info->insn_addr & 0xffffffff00000000) == 0xffffffff00000000) {
        pstack = get_process_stack(cache->ttbr1);
        tstack = get_thread_stack(pstack, info->sp);
        
    } else {
        pstack = get_process_stack(cache->ttbr0);
        tstack = get_thread_stack(pstack, info->sp);
    }

    if (!tstack) {
        return;
    }

    g_mutex_lock(&pstack->lock);
    
    if (info->is_call) {
        if (tstack->depth < MAX_CALLSTACK_DEPTH - 1) {
            if (info->reg_name[0]) { // This is a blr instruction
                target_addr = read_gp_register(info->reg_name);
                if (target_addr == 0) {
                    target_addr = info->insn_addr; // fallback
                }
            } else { // This is a bl instruction
                target_addr = info->target;
            }
            
            tstack->entries[tstack->depth].addr = target_addr;
            tstack->entries[tstack->depth].symbol = sym;
            tstack->depth++;
        }
    } else if (info->is_ret && tstack->depth > 0) {
        tstack->depth--;
    }

    g_mutex_unlock(&pstack->lock);
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

        g_free(disas);
    }
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_autoptr(GString) report = g_string_new("Callstack Report By TTBR:\n");
    GHashTableIter iter;
    gpointer key, value;
    
    if (!process_stacks || !report) {
        return;
    }
    
    g_mutex_lock(&stacks_lock);
    g_hash_table_iter_init(&iter, process_stacks);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        uint64_t ttbr = (uint64_t)key;
        ProcessCallStack *pstack = (ProcessCallStack *)value;
        GHashTableIter tstack_iter;
        gpointer tstack_key, tstack_value;
        
        if (!pstack) {
            continue;
        }

        g_mutex_lock(&pstack->lock);
        
        g_hash_table_iter_init(&tstack_iter, pstack->thread_stacks);

        while (g_hash_table_iter_next(&tstack_iter, &tstack_key, &tstack_value)) {
            uint64_t sp = (uint64_t)tstack_key;
            ThreadCallStack *tstack = (ThreadCallStack *)tstack_value;
        
            g_string_append_printf(report, "\nTTBR 0x%" PRIx64 " SP 0x%" PRIx64 " callstack depth: %d\n",
                                   ttbr, sp, tstack->depth);
            
            if (tstack->depth > 0) {
                g_string_append_printf(report, "Current callstack:\n");
                for (int j = 0; j < tstack->depth; j++) {
                    g_string_append_printf(report,
                                      "  #%-2d 0x%" PRIx64 " in %s\n",
                                      j, tstack->entries[j].addr,
                                      tstack->entries[j].symbol);
                }
            }
            g_free(tstack->entries);
        }
        
        g_hash_table_destroy(pstack->thread_stacks);
        g_mutex_unlock(&pstack->lock);
        g_mutex_clear(&pstack->lock);
    }
    
    qemu_plugin_outs(report->str);
    
    /* Clean up vcpu caches */
    g_mutex_lock(&vcpu_caches_lock);
    g_hash_table_iter_init(&iter, vcpu_caches);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        VCPUCache *cache = (VCPUCache *)value;
        if (cache) {
            g_mutex_clear(&cache->lock);
        }
    }
    g_hash_table_destroy(vcpu_caches);
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

    vcpu_caches = g_hash_table_new_full(NULL, g_direct_equal, NULL, g_free);
    if (!vcpu_caches) {
        return -1;
    }
    g_mutex_init(&vcpu_caches_lock);

    process_stacks = g_hash_table_new_full(NULL, g_direct_equal, NULL, g_free);
    if (!process_stacks) {
        g_hash_table_destroy(vcpu_caches);
        g_mutex_clear(&vcpu_caches_lock);
        return -1;
    }
    g_mutex_init(&stacks_lock);
    
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    
    return 0;
}
