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

#ifndef QEMU_NXP_S32G_H
#define QEMU_NXP_S32G_H

#include "hw/sysbus.h"
#include "hw/cpu/cluster.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/char/pl011.h"
#include "qom/object.h"


#define S32G_NUM_UART 2

#define S32G_MIN_ACPUS  4
#define S32G_MAX_ACPUS  8

#define TYPE_NXP_S32G_MACHINE   MACHINE_TYPE_NAME("nxp-s32g")
OBJECT_DECLARE_SIMPLE_TYPE(NxpS32gState, NXP_S32G_MACHINE)

struct NxpS32gState {
    MachineState parent_obj;
    
    CPUClusterState ap_cluster;
    ARMCPU ap_cpu[S32G_MAX_ACPUS];
    GICv3State gic;

    PL011State uart[S32G_NUM_UART];

    MemoryRegion *mr;

    struct arm_boot_info binfo;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t gic_phandle;
    int psci_conduit; // HVC

    bool secure; // disabled
    bool virt; // disabled
};

/* Number of external interrupt lines to configure the GIC with */
#define S32G_NUM_IRQ 256

/* PPI interrupts */
#define S32G_GIC_MAINT_IRQ        9
#define S32G_TIMER_NS_EL2_IRQ     10
#define S32G_TIMER_VIRT_IRQ       11
#define S32G_TIMER_HYP_VIRT_IRQ   12
#define S32G_TIMER_S_EL1_IRQ      13
#define S32G_TIMER_NS_EL1_IRQ     14

/* Memory Map Constants */
#define S32G_PHYSMEM_BASE         0x80000000

#define MM_GIC_DIST               0x50800000
#define MM_GIC_DIST_SIZE          0x10000
#define MM_GIC_REDIST             0x50880000
#define MM_GIC_REDIST_SIZE        0x80000
#define MM_UART0                  0x401c8000
#define MM_UART1                  0x401cc000

/* UART */
#define S32G_NUM_UART             2
#define S32G_UART0_IRQ_0          82
#define S32G_UART1_IRQ_0          83

#endif /* QEMU_NXP_S32G_H */
