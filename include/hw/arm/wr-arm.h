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

#define WR_MIN_ACPUS        4
#define WR_MAX_ACPUS        8

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

/* Memory Map Constants */
#define WR_PHYSMEM_BASE         0x80000000

#define MM_GIC_DIST             0x50800000
#define MM_GIC_DIST_SIZE        0x10000
#define MM_GIC_REDIST           0x50900000
#define MM_GIC_REDIST_SIZE      0x200000
#define MM_UART0                0x401c8000
#define MM_VIRTIO_BASE          0x0a000000
#define MM_VIRTIO_SIZE          0x200

/* UART */
#define WR_UART0_IRQ_0          82

/* virtio */
#define NUM_VIRTIO_TRANSPORTS   4
#define WR_VIRTIO_IRQ_BASE      0x68

#define TYPE_WR_ARM_MACHINE     MACHINE_TYPE_NAME("wr-arm")

OBJECT_DECLARE_SIMPLE_TYPE(WrArmMachineState, WR_ARM_MACHINE)

struct WrArmMachineState {
    MachineState parent_obj;

    CPUClusterState cpu_cluster;
    ARMCPU cpus[WR_MAX_ACPUS];
    GICv3State gic;
    qemu_irq irqmap[WR_NUM_IRQ];

    PL011State uart;

    MemoryRegion *mr;

    struct arm_boot_info binfo;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t gic_phandle;
    int psci_conduit; // HVC

    bool secure; // disabled
    bool virt; // disabled
};

#endif /* QEMU_WR_ARM_H */
