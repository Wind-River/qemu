/*
 * devaccess.c, TCG plugin to trace device access
 *
 * Copyright (c) 2024 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct {
    uint64_t cpu_id;
    uint64_t addr;
    uint64_t pc;
    bool write;
    const char *name;
} TraceEntry;

GPtrArray *traces;
static GMutex tracesLock;

static void flush_traces(void) {
    GString *out = g_string_new(NULL);
    int i;

    g_mutex_lock(&tracesLock);

    for (i = 0; i < traces->len; i++) {
        TraceEntry *entry = g_ptr_array_index(traces, i);
        g_string_append_printf(out, "%ld, 0x%08"PRIx64", %s, 0x%08"PRIx64"\n",
                               entry->cpu_id, entry->addr, entry->name, entry->pc);
    }
    g_ptr_array_remove_range(traces, 0, traces->len);

    g_mutex_unlock(&tracesLock);

    qemu_plugin_outs(out->str);
    g_string_free(out, true);
}

static void vcpu_mem_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                           uint64_t vaddr, void *udata)
{
    TraceEntry *entry;
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);

    if (!hwaddr || !qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    }

    entry = g_new0(TraceEntry, 1);
    entry->cpu_id = cpu_index;
    entry->addr = qemu_plugin_hwaddr_phys_addr(hwaddr);
    entry->pc = (uint64_t)udata;
    entry->write = qemu_plugin_mem_is_store(meminfo);
    entry->name = qemu_plugin_hwaddr_device_name(hwaddr);

    g_mutex_lock(&tracesLock);
    g_ptr_array_add(traces, entry);
    g_mutex_unlock(&tracesLock);
}


static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;
    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        gpointer udata = (gpointer)qemu_plugin_insn_vaddr(insn);
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         QEMU_PLUGIN_MEM_RW, udata);
    }

    flush_traces();
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    flush_traces();
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    if (!info->system_emulation) {
        fprintf(stderr, "devaccess: plugin only supports system emulation\n");
        return -1;
    }

    traces = g_ptr_array_new();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
