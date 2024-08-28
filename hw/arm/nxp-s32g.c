/*
 * nxp-s32g.c, NXP S32G emulation
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

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qapi/qmp/qlist.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "cpu.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/arm/nxp-s32g.h"
#include "hw/arm/fdt.h"
#include "hw/misc/unimp.h"
#include "exec/address-spaces.h"


static void *s32g_get_dtb(const struct arm_boot_info *binfo)
{
    const NxpS32gState *s = container_of(binfo, NxpS32gState, binfo);
    MachineState *ms = MACHINE(s);

    return ms->fdt;
}

static bool s32g_get_secure(Object *obj, Error **errp)
{
    NxpS32gState *s = NXP_S32G_MACHINE(obj);

    return s->secure;
}

static void s32g_set_secure(Object *obj, bool value, Error **errp)
{
    NxpS32gState *s = NXP_S32G_MACHINE(obj);

    s->secure = value;
}

static bool s32g_get_virt(Object *obj, Error **errp)
{
    NxpS32gState *s = NXP_S32G_MACHINE(obj);

    return s->virt;
}

static void s32g_set_virt(Object *obj, bool value, Error **errp)
{
    NxpS32gState *s = NXP_S32G_MACHINE(obj);

    s->virt = value;
}

static void s32g_create_cpus(NxpS32gState *s)
{
    int i;
    const MachineState *ms = MACHINE(s);

    object_initialize_child(OBJECT(s), "cpu-cluster", &s->ap_cluster, 
                            TYPE_CPU_CLUSTER);
    qdev_prop_set_uint32(DEVICE(&s->ap_cluster), "cluster-id", 0);
    for (i = 0; i < ms->smp.cpus; i++) {
        Object *obj;

        object_initialize_child(OBJECT(&s->ap_cluster),
                                "cpu[*]", &s->ap_cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
        obj = OBJECT(&s->ap_cpu[i]);
        if (i) {
            /* Secondary CPUs start in powered-down state */
            object_property_set_bool(obj, "start-powered-off", true,
                                     &error_abort);
        }

        if (!s->secure) {
            object_property_set_bool(obj, "has_el3", false, NULL);
        }

        if (!s->virt && object_property_find(obj, "has_el2")) {
            object_property_set_bool(obj, "has_el2", false, NULL);
        }

        object_property_set_int(obj, "core-count", S32G_CORES_PER_CPU,
                                &error_abort);
        object_property_set_link(obj, "memory", OBJECT(s->mr),
                                 &error_abort);
        qdev_realize(DEVICE(obj), NULL, &error_fatal);
    }

    qdev_realize(DEVICE(&s->ap_cluster), NULL, &error_fatal);
}

static void s32g_create_gic(NxpS32gState *s, qemu_irq *irqmap)
{
    static const uint64_t addrs[] = {
        MM_GIC_DIST,
        MM_GIC_REDIST
    };
    SysBusDevice *gicbusdev;
    DeviceState *gicdev;
    uint32_t redist0_cap, redist0_cnt;
    QList *redist_region_count;
    const MachineState *ms = MACHINE(s);
    int nr_cpus = ms->smp.cpus;
    int i;

    object_initialize_child(OBJECT(s), "gic", &s->gic,
                            gicv3_class_name());
    gicbusdev = SYS_BUS_DEVICE(&s->gic);
    gicdev = DEVICE(&s->gic);
    qdev_prop_set_uint32(gicdev, "revision", 3);
    qdev_prop_set_uint32(gicdev, "num-cpu", nr_cpus);
    // Always GIC_INTERNAL (32) internal interrupts
    qdev_prop_set_uint32(gicdev, "num-irq", S32G_NUM_IRQ + GIC_INTERNAL);

    /* 
     * GICv3 has two 64KB frames per CPU
     * Divide the gic redist size by 0x20000 to see how many
     * redist regions we should advertise.
     */
    redist0_cap = MM_GIC_REDIST_SIZE / GICV3_REDIST_SIZE;
    redist0_cnt = MIN(nr_cpus, redist0_cap);
    redist_region_count = qlist_new();
    qlist_append_int(redist_region_count, redist0_cnt);
    qdev_prop_set_array(gicdev, "redist-region-count", redist_region_count);

    qdev_prop_set_bit(gicdev, "has-security-extensions", true);

    sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);

    for (i = 0; i < ARRAY_SIZE(addrs); i++) {
        MemoryRegion *mr;

        mr = sysbus_mmio_get_region(gicbusdev, i);
        memory_region_add_subregion(s->mr, addrs[i], mr);
    }

    for (i = 0; i < nr_cpus; i++) {
        DeviceState *cpudev = DEVICE(&s->ap_cpu[i]);
        int ppibase = S32G_NUM_IRQ + i * GIC_INTERNAL + GIC_NR_SGIS;
        qemu_irq maint_irq;
        int ti;
        /* Mapping from the output timer irq lines from the CPU to the
         * GIC PPI inputs.
         */
        const int timer_irq[] = {
            [GTIMER_SEC]  = S32G_TIMER_S_EL1_IRQ,
            [GTIMER_PHYS] = S32G_TIMER_NS_EL1_IRQ,
            [GTIMER_VIRT] = S32G_TIMER_VIRT_IRQ,
            [GTIMER_HYP]  = S32G_TIMER_NS_EL2_IRQ,
        };

        for (ti = 0; ti < ARRAY_SIZE(timer_irq); ti++) {
            qdev_connect_gpio_out(cpudev, ti,
                                  qdev_get_gpio_in(gicdev,
                                                   ppibase + timer_irq[ti]));
        }
        maint_irq = qdev_get_gpio_in(gicdev,
                                        ppibase + S32G_GIC_MAINT_IRQ);
        qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt",
                                    0, maint_irq);
        sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(gicbusdev, i + nr_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(gicbusdev, i + 2 * nr_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(gicbusdev, i + 3 * nr_cpus,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
    }

    for (i = 0; i < S32G_NUM_IRQ; i++) {
        irqmap[i] = qdev_get_gpio_in(gicdev, i);
    }
}

static void s32g_create_uart(NxpS32gState *s, qemu_irq *irqmap)
{
    int i;
    static const int irqs[] = { S32G_UART0_IRQ_0, S32G_UART1_IRQ_0};
    static const uint64_t addrs[] = { MM_UART0, MM_UART1 };

    for (i = 0; i < ARRAY_SIZE(s->uart); i++) {
        char *name = g_strdup_printf("uart%d", i);
        DeviceState *dev;
        MemoryRegion *mr;

        object_initialize_child(OBJECT(s), name, &s->uart[i],
                                TYPE_FSL_LINFLEX);
        dev = DEVICE(&s->uart[i]);
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

        mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
        memory_region_add_subregion(s->mr, addrs[i], mr);

        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irqmap[irqs[i]]);
        g_free(name);
    }
}

static void s32g_create_flexcan(NxpS32gState *s, qemu_irq *irqmap)
{
    int i;
    static const int irqs[] = { S32G_FLEXCAN0_IRQ_STATE, S32G_FLEXCAN1_IRQ_STATE };
    static const uint64_t addrs[] = { MM_FLEXCAN0, MM_FLEXCAN1 };

    for (i = 0; i < ARRAY_SIZE(s->flexcan); i++) {
        char *name = g_strdup_printf("flexcan%d", i);
        SysBusDevice *dev;
        MemoryRegion *mr;

        object_initialize_child(OBJECT(s), name, &s->flexcan[i],
                                TYPE_FSL_FLEXCAN);
        dev = SYS_BUS_DEVICE(&s->flexcan[i]);


        object_property_set_link(OBJECT(&s->flexcan[i]), "canbus",
                                 OBJECT(s->canbus[i]), &error_abort);

        sysbus_realize(dev, &error_fatal);

        mr = sysbus_mmio_get_region(dev, 0);
        memory_region_add_subregion(s->mr, addrs[i], mr);

        /* each flexcan has 4 irqs */
        sysbus_connect_irq(dev, 0, irqmap[irqs[i]]);    // IRQ_STATE
        sysbus_connect_irq(dev, 0, irqmap[irqs[i]+1]);  // IRQ_BERR
        sysbus_connect_irq(dev, 0, irqmap[irqs[i]+2]);  // IRQ_MB_0_7
        sysbus_connect_irq(dev, 0, irqmap[irqs[i]+3]);  // IRQ_MB_8_127
        g_free(name);
    }
}

static void create_virtio_devices(NxpS32gState *s)
{
    int i;
    MachineState *ms = MACHINE(s);

    for (i = 0; i < NUM_VIRTIO_TRANSPORTS; i++) {
        int irq = S32G_VIRTIO_IRQ_BASE + i;
        hwaddr base = MM_VIRTIO_BASE + i * MM_VIRTIO_SIZE;

        sysbus_create_simple("virtio-mmio", base,
                             qdev_get_gpio_in(DEVICE(&s->gic), irq));
    }

    /* We add dtb nodes in reverse order so that they appear in the finished
     * device tree lowest address first.
     *
     * Note that this mapping is independent of the loop above. The previous
     * loop influences virtio device to virtio transport assignment, whereas
     * this loop controls how virtio transports are laid out in the dtb.
     */
    for (i = NUM_VIRTIO_TRANSPORTS - 1; i >= 0; i--) {
        char *nodename;
        int irq = S32G_VIRTIO_IRQ_BASE + i;
        hwaddr base = MM_VIRTIO_BASE + i * MM_VIRTIO_SIZE;

        nodename = g_strdup_printf("/virtio_mmio@%" PRIx64, base);
        qemu_fdt_add_subnode(ms->fdt, nodename);
        qemu_fdt_setprop_string(ms->fdt, nodename,
                                "compatible", "virtio,mmio");
        qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
                                     2, base, 2, MM_VIRTIO_SIZE);
        qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irq,
                               GIC_FDT_IRQ_FLAGS_EDGE_LO_HI);
        qemu_fdt_setprop(ms->fdt, nodename, "dma-coherent", NULL, 0);
        g_free(nodename);
    }
}

static void fdt_add_flexcan_nodes(NxpS32gState *s)
{
    const MachineState *ms = MACHINE(s);
    static const int irqs[] = { S32G_FLEXCAN0_IRQ_STATE, S32G_FLEXCAN1_IRQ_STATE };
    static const uint64_t addrs[] = { MM_FLEXCAN0, MM_FLEXCAN1 };
    const char compat[] = "fsl,flexcan";
    const char clocknames[] = "per";
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->flexcan); i++) {
        char *name = g_strdup_printf("/flexcan@%" PRIx64, addrs[i]);
        qemu_fdt_add_subnode(ms->fdt, name);
        qemu_fdt_setprop(ms->fdt, name, "compatible", compat, sizeof(compat));

        qemu_fdt_setprop_sized_cells(ms->fdt, name, "reg", 2, addrs[i],
                                     2, 0xa000);
        /* there are 4 irqs per flexcan */
        qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irqs[i],
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);
        qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irqs[i]+1,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);
        qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irqs[i]+2,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);
        qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irqs[i]+3,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);

        qemu_fdt_setprop_cells(ms->fdt, name, "clocks", s->flexcan_clock_phandle);
        qemu_fdt_setprop(ms->fdt, name, "clock-names",
                         clocknames, sizeof(clocknames));
    }
}

static void fdt_add_cpu_nodes(NxpS32gState *s)
{
    int i;
    const MachineState *ms = MACHINE(s);

    qemu_fdt_add_subnode(ms->fdt, "/cpus");
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#address-cells", 1);

    for (i = ms->smp.cpus - 1; i >= 0; i--) {
        char *name = g_strdup_printf("/cpus/cpu@%d", i);
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(i));

        qemu_fdt_add_subnode(ms->fdt, name);
        qemu_fdt_setprop_cell(ms->fdt, name, "reg", armcpu->mp_affinity);
        if (s->psci_conduit != QEMU_PSCI_CONDUIT_DISABLED) {
            qemu_fdt_setprop_string(ms->fdt, name, "enable-method", "psci");
        }
        qemu_fdt_setprop_string(ms->fdt, name, "device_type", "cpu");
        qemu_fdt_setprop_string(ms->fdt, name, "compatible",
                                armcpu->dtb_compatible);
        g_free(name);
    }
}

static void fdt_add_uart_nodes(NxpS32gState *s)
{
    const MachineState *ms = MACHINE(s);
    uint64_t addrs[] = { MM_UART1, MM_UART0 };
    unsigned int irqs[] = { S32G_UART1_IRQ_0, S32G_UART0_IRQ_0 };
    const char compat[] = "fsl,s32-linflexuart\0nxp,s32cc-linflexuart";
    const char clocknames[] = "ipg";
    int i;

    for (i = 0; i < ARRAY_SIZE(addrs); i++) {
        char *name = g_strdup_printf("/uart@%" PRIx64, addrs[i]);
        qemu_fdt_add_subnode(ms->fdt, name);
        qemu_fdt_setprop(ms->fdt, name, "compatible",
                         compat, sizeof(compat));

        qemu_fdt_setprop_sized_cells(ms->fdt, name, "reg",
                                     2, addrs[i], 2, 0x1000);
        qemu_fdt_setprop_cells(ms->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irqs[i],
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);

        qemu_fdt_setprop_cells(ms->fdt, name, "clocks",
                               s->clock_phandle, s->clock_phandle);
        qemu_fdt_setprop(ms->fdt, name, "clock-names",
                         clocknames, sizeof(clocknames));

        if (addrs[i] == MM_UART0) {
            /* Select UART0 as stdout  */
            qemu_fdt_setprop_string(ms->fdt, "/chosen", "stdout-path", name);
        }
        g_free(name);
    }
}

static void fdt_add_timer_nodes(NxpS32gState *s)
{
    MachineState *ms = MACHINE(s);
    const char compat[] = "arm,armv8-timer";
    uint32_t irqFlags = GIC_FDT_IRQ_FLAGS_LEVEL_HI;

    qemu_fdt_add_subnode(ms->fdt, "/timer");
    qemu_fdt_setprop(ms->fdt, "/timer", "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(ms->fdt, "/timer", "interrupts",
        GIC_FDT_IRQ_TYPE_PPI, S32G_TIMER_S_EL1_IRQ, irqFlags,
        GIC_FDT_IRQ_TYPE_PPI, S32G_TIMER_NS_EL1_IRQ, irqFlags,
        GIC_FDT_IRQ_TYPE_PPI, S32G_TIMER_VIRT_IRQ, irqFlags,
        GIC_FDT_IRQ_TYPE_PPI, S32G_TIMER_NS_EL2_IRQ, irqFlags);
}

static void fdt_add_gic_nodes(NxpS32gState *s)
{
    MachineState *ms = MACHINE(s);
    char *nodename;

    nodename = g_strdup_printf("/gic@%x", MM_GIC_DIST);

    s->gic_phandle = qemu_fdt_alloc_phandle(ms->fdt);

    // set top-level interrupt-parent to gic phandle
    qemu_fdt_setprop_cell(ms->fdt, "/", "interrupt-parent", s->gic_phandle);

    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
                           GIC_FDT_IRQ_TYPE_PPI,
                           INTID_TO_PPI(ARCH_GIC_MAINT_IRQ),
                           GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    qemu_fdt_setprop(ms->fdt, nodename, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "#interrupt-cells", 3);
    qemu_fdt_setprop_string(ms->fdt, nodename, "compatible", "arm,gic-v3");
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
                                 2, MM_GIC_DIST,
                                 2, MM_GIC_DIST_SIZE,
                                 2, MM_GIC_REDIST,
                                 2, MM_GIC_REDIST_SIZE);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", s->gic_phandle);

    g_free(nodename);
}

static void fdt_add_clk_nodes(NxpS32gState *s)
{
    MachineState *ms = MACHINE(s);

    s->clock_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    qemu_fdt_add_subnode(ms->fdt, "/ipg");
    qemu_fdt_setprop_string(ms->fdt, "/ipg", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(ms->fdt, "/ipg", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/ipg", "clock-frequency", 24000000);
    qemu_fdt_setprop_cell(ms->fdt, "/ipg", "phandle", s->clock_phandle);

    s->flexcan_clock_phandle = qemu_fdt_alloc_phandle(ms->fdt);
    qemu_fdt_add_subnode(ms->fdt, "/per");
    qemu_fdt_setprop_string(ms->fdt, "/per", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(ms->fdt, "/per", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(ms->fdt, "/per", "clock-frequency", 5000000);
    qemu_fdt_setprop_cell(ms->fdt, "/per", "phandle", s->flexcan_clock_phandle);
}

static void fdt_create(NxpS32gState *s)
{
    MachineState *ms = MACHINE(s);
    MachineClass *mc = MACHINE_GET_CLASS(s);
    void *fdt;

    fdt = create_device_tree(&s->fdt_size);
    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    ms->fdt = fdt;

    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_string(fdt, "/", "model", mc->desc);
    qemu_fdt_setprop_string(fdt, "/", "compatible", "nxp,s32g3");

    qemu_fdt_add_subnode(fdt, "/chosen");
}


static void nxp_s32g_init(MachineState *machine)
{
    NxpS32gState *s = NXP_S32G_MACHINE(machine);
    qemu_irq irqmap[S32G_NUM_IRQ];

    uint64_t ram_size = machine->ram_size;

    s->psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    s->mr = get_system_memory();

    s32g_create_cpus(s);

    fdt_create(s);
    fdt_add_cpu_nodes(s);
    fdt_add_clk_nodes(s);
    fdt_add_timer_nodes(s);


    /* add physical memory region to bus */
    /* todo: physmem starts at 0x8000_0000 but has a jump
     * after the first 2GB to 0x8_0000_0000 */
    memory_region_add_subregion(s->mr, S32G_PHYSMEM_BASE,
                                machine->ram);

    s32g_create_gic(s, irqmap);

    fdt_add_gic_nodes(s);

    // virtio transports must be created after gic
    create_virtio_devices(s);

    s32g_create_uart(s, irqmap);

    fdt_add_uart_nodes(s);

    s32g_create_flexcan(s, irqmap);

    fdt_add_flexcan_nodes(s);


    s->binfo.ram_size = ram_size;
    s->binfo.board_id = -1;
    s->binfo.loader_start = S32G_PHYSMEM_BASE;
    s->binfo.get_dtb = s32g_get_dtb;
    s->binfo.psci_conduit = s->psci_conduit;

    arm_load_kernel(&s->ap_cpu[0], machine, &s->binfo);
}

static void nxp_s32g_machine_instance_init(Object *obj)
{
    NxpS32gState *s = NXP_S32G_MACHINE(obj);

    /* default to secure mode disabled and virtualization (el2) disabled */
    s->secure = false;
    s->virt = false;

    object_property_add_link(obj, "canbus0", TYPE_CAN_BUS,
                             (Object **)&s->canbus[0],
                             object_property_allow_set_link,
                             0);

    object_property_add_link(obj, "canbus1", TYPE_CAN_BUS,
                             (Object **)&s->canbus[1],
                             object_property_allow_set_link,
                             0);
}

static void nxp_s32g_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "NXP S32G Virtual Development Board";
    mc->init = nxp_s32g_init;
    mc->min_cpus = S32G_MIN_ACPUS;
    mc->max_cpus = S32G_MAX_ACPUS;
    mc->default_cpus = S32G_MAX_ACPUS;
    mc->no_cdrom = true;
    mc->default_ram_id = "ddr";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a53");

    object_class_property_add_bool(oc, "secure", s32g_get_secure,
                                   s32g_set_secure);
    object_class_property_set_description(oc, "secure",
                                                "Set on/off to enable/disable the ARM "
                                                "Security Extensions (TrustZone)");

    object_class_property_add_bool(oc, "virtualization", s32g_get_virt,
                                   s32g_set_virt);
    object_class_property_set_description(oc, "virtualization",
                                          "Set on/off to enable/disable emulating a "
                                          "guest CPU which implements the ARM "
                                          "Virtualization Extensions");

}

static const TypeInfo nxp_s32g_machine_info = {
    .name = TYPE_NXP_S32G_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(NxpS32gState),
    .class_init = nxp_s32g_machine_class_init,
    .instance_init = nxp_s32g_machine_instance_init,
};

static void nxp_s32g_machine_init(void)
{
    type_register_static(&nxp_s32g_machine_info);
}

type_init(nxp_s32g_machine_init)
