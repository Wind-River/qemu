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
    GMutex lock;
} ProcessCallStack;

/* Per-vCPU TTBR cache */
typedef struct {
    uint64_t ttbr;         /* Cached TTBR value */
    bool ttbr_dirty;       /* Flag indicating if cache needs refresh */
    GMutex lock;          /* Lock for this cache entry */
} VCPUCache;

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
            stack->entries = g_malloc0(sizeof(CallStackEntry) * MAX_CALLSTACK_DEPTH);
            if (!stack->entries) {
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

static uint64_t read_current_ttbr(void)
{
    g_autoptr(GString) ttbr_reg_prefix = g_string_new("TTBR");
    GArray *reg_list = qemu_plugin_get_registers();
    uint64_t current_ttbr = 0;
    
    if (!reg_list) {
        return 0;
    }

    for (int i = 0; i < reg_list->len; i++) {
        qemu_plugin_reg_descriptor *desc = &g_array_index(reg_list, 
            qemu_plugin_reg_descriptor, i);
        if (strncmp(desc->name, ttbr_reg_prefix->str, ttbr_reg_prefix->len) == 0) {
            GByteArray *reg_buf = g_byte_array_new();
            int regsize = qemu_plugin_read_register(desc->handle, reg_buf);
            
            if (regsize > 0) {
                for (int j = regsize-1; j >= 0; j--) {
                    current_ttbr = (current_ttbr << 8) | reg_buf->data[j];
                }
            }
            g_byte_array_free(reg_buf, TRUE);
            break;
        }
    }
    g_array_free(reg_list, TRUE);
    return current_ttbr;
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

static uint64_t get_current_ttbr_cached(unsigned int cpu_index)
{
    VCPUCache *cache = get_vcpu_cache(cpu_index);
    uint64_t ttbr;
    
    if (!cache) {
        return read_current_ttbr();
    }
    
    g_mutex_lock(&cache->lock);
    if (cache->ttbr_dirty) {
        cache->ttbr = read_current_ttbr();
        cache->ttbr_dirty = false;
    }
    ttbr = cache->ttbr;
    g_mutex_unlock(&cache->lock);
    
    return ttbr;
}

static void vcpu_insn_exec(unsigned int cpu_index, void *udata)
{
    struct qemu_plugin_insn *insn = (struct qemu_plugin_insn *)udata;
    ProcessCallStack *stack;
    const char *sym;
    bool is_call, is_ret;

    if (!insn) {
        return;
    }
    
    /* Get flags from userdata */
    is_call = ((uintptr_t)udata & 1) == 1;
    is_ret = ((uintptr_t)udata & 2) == 2;

    if (!is_call && !is_ret) {
        return;
    }

    uint64_t current_ttbr = get_current_ttbr_cached(cpu_index);
    stack = get_process_stack(current_ttbr);
    if (!stack) {
        return;
    }
    
    g_mutex_lock(&stack->lock);
    
    if (is_call) {
        if (stack->depth < MAX_CALLSTACK_DEPTH - 1) {
            uint64_t pc = qemu_plugin_insn_vaddr(insn);
            stack->entries[stack->depth].addr = pc;
            sym = qemu_plugin_insn_symbol(insn);
            stack->entries[stack->depth].symbol = sym ? sym : "<unknown>";
            stack->depth++;
        }
    } else if (is_ret && stack->depth > 0) {
        stack->depth--;
    }
    
    g_mutex_unlock(&stack->lock);
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

        /* On AArch64:
           - bl/blr are call instructions
           - ret is return instruction 
           - msr ttbr changes address space */
        if (g_str_has_prefix(disas, "bl ") || g_str_has_prefix(disas, "blr ")) {
            uintptr_t tagged = (uintptr_t)insn | 1;  // Mark as call
            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                                 QEMU_PLUGIN_CB_R_REGS,
                                                 (void *)tagged);
        } else if (g_str_has_prefix(disas, "ret")) {
            uintptr_t tagged = (uintptr_t)insn | 2;  // Mark as return
            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                                 QEMU_PLUGIN_CB_R_REGS,
                                                 (void *)tagged);
        } else if (g_str_has_prefix(disas, "msr ttbr")) {
            qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_ttbr_exec,
                                                 QEMU_PLUGIN_CB_NO_REGS,
                                                 NULL);
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
        ProcessCallStack *stack = (ProcessCallStack *)value;
        
        if (!stack) {
            continue;
        }
        
        g_mutex_lock(&stack->lock);
        
        g_string_append_printf(report, "\nTTBR 0x%" PRIx64 " callstack depth: %d\n",
                             ttbr, stack->depth);
        
        if (stack->depth > 0) {
            g_string_append_printf(report, "Current callstack:\n");
            for (int j = 0; j < stack->depth; j++) {
                g_string_append_printf(report,
                                  "  #%-2d 0x%" PRIx64 " in %s\n",
                                  j, stack->entries[j].addr,
                                  stack->entries[j].symbol);
            }
        }
        
        g_mutex_unlock(&stack->lock);
        g_mutex_clear(&stack->lock);
        g_free(stack->entries);
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
