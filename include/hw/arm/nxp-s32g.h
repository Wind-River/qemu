/*
 * nxp-s32g.h, NXP S32G emulation
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

#ifndef QEMU_NXP_S32G_H
#define QEMU_NXP_S32G_H

#include "hw/sysbus.h"
#include "hw/cpu/cluster.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/char/pl011.h"
#include "hw/char/fsl-linflex.h"
#include "hw/net/fsl_flexcan.h"
#include "qom/object.h"


#define S32G_MIN_ACPUS  4
#define S32G_MAX_ACPUS  8

#define S32G_CORES_PER_CPU 1

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
#define MM_GIC_REDIST             0x50900000
#define MM_GIC_REDIST_SIZE        0x200000
#define MM_FLEXCAN0               0x401b4000
#define MM_FLEXCAN1               0x401be000
#define MM_UART0                  0x401c8000
#define MM_UART1                  0x401cc000
#define MM_VIRTIO_BASE            0x0a000000
#define MM_VIRTIO_SIZE            0x200

/* UART */
#define S32G_NUM_UART             2
#define S32G_UART0_IRQ_0          82
#define S32G_UART1_IRQ_0          83

/* FlexCAN */
#define S32G_NUM_FLEXCAN            2   // there are actually 4
#define S32G_FLEXCAN0_IRQ_STATE     37
#define S32G_FLEXCAN0_IRQ_BERR      38
#define S32G_FLEXCAN0_IRQ_MB_0_7    39
#define S32G_FLEXCAN0_IRQ_MB_8_127  40
#define S32G_FLEXCAN1_IRQ_STATE     41
#define S32G_FLEXCAN1_IRQ_BERR      42
#define S32G_FLEXCAN1_IRQ_MB_0_7    43
#define S32G_FLEXCAN1_IRQ_MB_8_127  44

/* virtio */
#define NUM_VIRTIO_TRANSPORTS     4
#define S32G_VIRTIO_IRQ_BASE      0x68

#define TYPE_NXP_S32G_MACHINE   MACHINE_TYPE_NAME("nxp-s32g")
OBJECT_DECLARE_SIMPLE_TYPE(NxpS32gState, NXP_S32G_MACHINE)

struct NxpS32gState {
    MachineState parent_obj;

    CPUClusterState ap_cluster;
    ARMCPU ap_cpu[S32G_MAX_ACPUS];
    GICv3State gic;

    FslLinflexState uart[S32G_NUM_UART];

    CanBusState *canbus[S32G_NUM_FLEXCAN];
    FslFlexCanState flexcan[S32G_NUM_FLEXCAN];

    MemoryRegion *mr;

    struct arm_boot_info binfo;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t flexcan_clock_phandle;
    uint32_t gic_phandle;
    int psci_conduit; // HVC

    bool secure; // disabled
    bool virt; // disabled
};

#endif /* QEMU_NXP_S32G_H */
