/*
 *
 * Copyright (C) 2024 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QEMU_WR_ARM_H
#define QEMU_WR_ARM_H

#include "hw/sysbus.h"
#include "hw/cpu/cluster.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/char/pl011.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/arm/fdt.h"
#include "hw/misc/unimp.h"
#include "qom/object.h"

#define WR_ARM_COMPATIBLE   "nxp,s32g3"

#define WR_MAX_CPUS         64

#define WR_CORES_PER_CPU    1

/* Number of external interrupt lines to configure the GIC with */
#define WR_NUM_IRQ          256

/* PPI interrupts */
#define WR_GIC_MAINT_IRQ        9
#define WR_TIMER_NS_EL2_IRQ     10
#define WR_TIMER_VIRT_IRQ       11
#define WR_TIMER_HYP_VIRT_IRQ   12
#define WR_TIMER_S_EL1_IRQ      13
#define WR_TIMER_NS_EL1_IRQ     14

/* UART */
#define WR_UART0_IRQ_0          82

/* virtio */
#define NUM_VIRTIO_TRANSPORTS   4
#define WR_VIRTIO_IRQ_BASE      0x68


struct WrArmMachineState {
    MachineState parent_obj;

    CPUClusterState cpu_cluster;
    ARMCPU cpus[WR_MAX_CPUS];
    GICv3State gic;
    qemu_irq irqmap[WR_NUM_IRQ];

    PL011State uart;

    MemoryRegion mem_lo;
    MemoryRegion mem_hi;

    struct arm_boot_info binfo;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t gic_phandle;
};

#define TYPE_WR_ARM_MACHINE     MACHINE_TYPE_NAME("wr-arm")
OBJECT_DECLARE_SIMPLE_TYPE(WrArmMachineState, WR_ARM_MACHINE)

enum {
    WR_LO_MEM,
    WR_UART0,
    WR_GIC_DIST,
    WR_GIC_REDIST,
    WR_HI_MEM,
};

static const MemMapEntry wr_memmap[] = {
    [WR_UART0] =            { 0x401c8000, 0x00001000 },
    [WR_GIC_DIST] =         { 0x50800000, 0x00010000 },
    [WR_GIC_REDIST] =       { 0x50900000, 0x00200000 },
    [WR_LO_MEM] =           { 0x80000000, 0x80000000 },
    [WR_HI_MEM] =           { 0x100000000, 0x0 }
};


#endif /* QEMU_WR_ARM_H */
