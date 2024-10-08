/*
 * wr-arm.c, Generic arm machine emulation
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
#include "exec/address-spaces.h"

#include "hw/arm/wr-arm.h"

static void *wr_arm_get_dtb(const struct arm_boot_info *binfo)
{
    const WrArmMachineState *s = container_of(binfo, WrArmMachineState, binfo);
    MachineState *ms = MACHINE(s);

    return ms->fdt;
}

static void wr_arm_create_virtio_transports(MachineState *machine)
{
    WrArmMachineState *s = WR_ARM_MACHINE(machine);
    const char compat[] = "virtio,mmio";
    int num_transports = wr_memmap[WR_VIRTIO].size / WR_VIRTIO_SZ;
    int i;

    /* Create transports in reverse order so they appear in the
     * finished device tree lowest address first. */
    for (i = num_transports - 1; i >= 0; i--) {
        char *name;
        int irq = WR_VIRTIO_IRQ_BASE + i;
        hwaddr base = wr_memmap[WR_VIRTIO].base + (i * WR_VIRTIO_SZ);
        sysbus_create_simple("virtio-mmio", base,
                             qdev_get_gpio_in(DEVICE(&s->gic), irq));

        name = g_strdup_printf("/virtio_mmio@%" PRIx64, base);
        qemu_fdt_add_subnode(machine->fdt, name);
        qemu_fdt_setprop_string(machine->fdt, name, "compatible", compat);
        qemu_fdt_setprop_sized_cells(machine->fdt, name, "reg", 2, base, 2, WR_VIRTIO_SZ);
        qemu_fdt_setprop_cells(machine->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irq,
                               GIC_FDT_IRQ_FLAGS_EDGE_LO_HI);
        qemu_fdt_setprop(machine->fdt, name, "dma-coherent", NULL, 0);
        g_free(name);
    }
}

static void wr_arm_create_uart(MachineState *machine)
{
    DeviceState *dev;
    MemoryRegion *mr;
    char *name = g_strdup_printf("uart0");
    char *nodename = g_strdup_printf("/uart0");
    const char compat[] = "arm,pl011\0arm,sbsa-uart";
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    /* realize the PL011 uart device */
    object_initialize_child(OBJECT(s), name, &s->uart,
                            TYPE_PL011);
    dev = DEVICE(&s->uart);
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_add_subregion(get_system_memory(), wr_memmap[WR_UART0].base, mr);

    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, s->irqmap[WR_UART0_IRQ_0]);

    /* add device tree nodes for uart */
    qemu_fdt_add_subnode(machine->fdt, nodename);
    qemu_fdt_setprop_cell(machine->fdt, nodename, "current-speed", 115200);
    qemu_fdt_setprop_cells(machine->fdt, nodename, "clocks",
                           s->clock_phandle, s->clock_phandle);
    qemu_fdt_setprop(machine->fdt, nodename, "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(machine->fdt, nodename, "interrupts",
                           GIC_FDT_IRQ_TYPE_SPI,
                           WR_UART0_IRQ_0,
                           GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    qemu_fdt_setprop_sized_cells(machine->fdt, nodename, "reg", 2,
                                 wr_memmap[WR_UART0].base, 2, 0x1000);
    qemu_fdt_setprop(machine->fdt, nodename, "u-boot,dm-pre-reloc", NULL, 0);

    /* Set uart0 as the default stdout device */
    qemu_fdt_setprop_string(machine->fdt, "/chosen", "stdout-path", nodename);
    g_free(name);
    g_free(nodename);
}

/* Create the system timer for the machine.
 * Since this is an arm system, we assume the use
 * of the arm generic timer to implement the system
 * timer.
 *
 * Create the device tree node for the timer. We do
 * not need to create any devices here.
 */
static void wr_arm_create_timer(MachineState *machine)
{
    const char compat[] = "arm,armv8-timer";

    qemu_fdt_add_subnode(machine->fdt, "/timer");
    qemu_fdt_setprop(machine->fdt, "/timer", "compatible", compat, sizeof(compat));
    qemu_fdt_setprop_cells(machine->fdt, "/timer", "interrupts",
        GIC_FDT_IRQ_TYPE_PPI, WR_TIMER_S_EL1_IRQ, GIC_FDT_IRQ_FLAGS_LEVEL_HI,
        GIC_FDT_IRQ_TYPE_PPI, WR_TIMER_NS_EL1_IRQ, GIC_FDT_IRQ_FLAGS_LEVEL_HI,
        GIC_FDT_IRQ_TYPE_PPI, WR_TIMER_VIRT_IRQ, GIC_FDT_IRQ_FLAGS_LEVEL_HI,
        GIC_FDT_IRQ_TYPE_PPI, WR_TIMER_NS_EL2_IRQ, GIC_FDT_IRQ_FLAGS_LEVEL_HI);
}

/*
 * Create the peripheral clock for the machine.
 * We do not actually emulate the clock input to
 * peripheral devices, so we don't realize any new
 * devices here.
 *
 * Create the device tree node for the clock and allocate
 * and store the phandle so we can reference it later.
 */
static void wr_arm_create_clock(MachineState *machine)
{
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    s->clock_phandle = qemu_fdt_alloc_phandle(machine->fdt);
    qemu_fdt_add_subnode(machine->fdt, "/clk");
    qemu_fdt_setprop_string(machine->fdt, "/clk", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(machine->fdt, "/clk", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(machine->fdt, "/clk", "clock-frequency", 24000000);
    qemu_fdt_setprop_cell(machine->fdt, "/clk", "phandle", s->clock_phandle);
}

static void fdt_add_gic_nodes(WrArmMachineState *s)
{
    MachineState *ms = MACHINE(s);
    char *nodename;

    nodename = g_strdup_printf("/gic@%lx", wr_memmap[WR_GIC_DIST].base);

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
                                 2, wr_memmap[WR_GIC_DIST].base,
                                 2, wr_memmap[WR_GIC_DIST].size,
                                 2, wr_memmap[WR_GIC_REDIST].base,
                                 2, wr_memmap[WR_GIC_REDIST].size);
    qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", s->gic_phandle);

    g_free(nodename);
}

static inline int arm_gic_ppi_index(int cpu_nr, int ppi_index)
{
    return WR_NUM_IRQ + GIC_NR_SGIS + cpu_nr * GIC_INTERNAL + ppi_index;
}

static void wr_arm_create_gic(MachineState *machine)
{
    static const uint64_t addrs[] = {
        wr_memmap[WR_GIC_DIST].base,
        wr_memmap[WR_GIC_REDIST].base
    };
    SysBusDevice *gicbusdev;
    DeviceState *gicdev;
    uint32_t redist0_cap, redist0_cnt;
    int i;
    QList *redist_region_count;
    WrArmMachineState *s = WR_ARM_MACHINE(machine);
    int nr_cpus = machine->smp.cpus;

    object_initialize_child(OBJECT(s), "gic", &s->gic,
                            gicv3_class_name());
    gicbusdev = SYS_BUS_DEVICE(&s->gic);
    gicdev = DEVICE(&s->gic);
    qdev_prop_set_uint32(gicdev, "revision", 3);
    qdev_prop_set_uint32(gicdev, "num-cpu", nr_cpus);
    // Always GIC_INTERNAL (32) internal interrupts
    qdev_prop_set_uint32(gicdev, "num-irq", WR_NUM_IRQ + GIC_INTERNAL);

    /*
     * GICv3 has two 64KB frames per CPU
     * Divide the gic redist size by 0x20000 to see how many
     * redist regions we should advertise.
     */
    redist0_cap = wr_memmap[WR_GIC_REDIST].size / GICV3_REDIST_SIZE;
    redist0_cnt = MIN(nr_cpus, redist0_cap);
    redist_region_count = qlist_new();
    qlist_append_int(redist_region_count, redist0_cnt);
    qdev_prop_set_array(gicdev, "redist-region-count", redist_region_count);

    qdev_prop_set_bit(gicdev, "has-security-extensions", true);

    sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);

    for (i = 0; i < ARRAY_SIZE(addrs); i++) {
        MemoryRegion *mr;

        mr = sysbus_mmio_get_region(gicbusdev, i);
        memory_region_add_subregion(get_system_memory(), addrs[i], mr);
    }

    for (i = 0; i < nr_cpus; i++) {
        DeviceState *cpudev = DEVICE(&s->cpus[i]);
        qemu_irq maint_irq;

        /* Map the output timer irq lines from the CPU to the
         * GIC PPI inputs.
         */
        qdev_connect_gpio_out(cpudev, GTIMER_PHYS,
                              qdev_get_gpio_in(gicdev,
                                               arm_gic_ppi_index(i, WR_TIMER_NS_EL1_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_VIRT,
                              qdev_get_gpio_in(gicdev,
                                               arm_gic_ppi_index(i, WR_TIMER_VIRT_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_HYP,
                              qdev_get_gpio_in(gicdev,
                                               arm_gic_ppi_index(i, WR_TIMER_NS_EL2_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_SEC,
                              qdev_get_gpio_in(gicdev,
                                               arm_gic_ppi_index(i, WR_TIMER_S_EL1_IRQ)));
        maint_irq = qdev_get_gpio_in(gicdev,
                                     arm_gic_ppi_index(i, WR_GIC_MAINT_IRQ));
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

    for (i = 0; i < WR_NUM_IRQ; i++) {
        s->irqmap[i] = qdev_get_gpio_in(gicdev, i);
    }

    fdt_add_gic_nodes(s);
}

static void wr_arm_create_cpus(MachineState *machine)
{
    int i;
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    /* realize the cpu cluster and cpus */
    object_initialize_child(OBJECT(s), "cpu-cluster", &s->cpu_cluster,
                            TYPE_CPU_CLUSTER);

    qdev_prop_set_uint32(DEVICE(&s->cpu_cluster), "cluster-id", 0);

    /* create CPUs in forward order */
    for (i = 0; i < machine->smp.cpus; i++) {
        Object *obj;

        object_initialize_child(OBJECT(&s->cpu_cluster), "cpu[*]",
                                &s->cpus[i],
                                machine->cpu_type);
        obj = OBJECT(&s->cpus[i]);
        if (i) {
            /* start APs in powered down state */
            object_property_set_bool(obj, "start-powered-off", true,
                                     &error_abort);
        }

        object_property_set_bool(obj, "has_el3", false, NULL);

        if (object_property_find(obj, "has_el2")) {
            object_property_set_bool(obj, "has_el2", false, NULL);
        }

        object_property_set_link(obj, "memory", OBJECT(get_system_memory()),
                                 &error_abort);
        qdev_realize(DEVICE(obj), NULL, &error_fatal);
    }

    qdev_realize(DEVICE(&s->cpu_cluster), NULL, &error_fatal);

    /* add FDT nodes for CPUs in reverse order */
    qemu_fdt_add_subnode(machine->fdt, "/cpus");
    qemu_fdt_setprop_cell(machine->fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(machine->fdt, "/cpus", "#address-cells", 1);

    for (i = machine->smp.cpus - 1; i >= 0; i--) {
        char *name = g_strdup_printf("/cpus/cpu@%d", i);
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(i));

        qemu_fdt_add_subnode(machine->fdt, name);
        qemu_fdt_setprop_cell(machine->fdt, name, "reg", armcpu->mp_affinity);
        qemu_fdt_setprop_string(machine->fdt, name, "enable-method", "psci");
        qemu_fdt_setprop_string(machine->fdt, name, "device_type", "cpu");
        qemu_fdt_setprop_string(machine->fdt, name, "compatible", armcpu->dtb_compatible);
    }
}

/*
 * Create a device tree for this machine
 */
static void fdt_create(MachineState *machine)
{
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    WrArmMachineState *s = WR_ARM_MACHINE(machine);
    void *fdt;

    fdt = create_device_tree(&s->fdt_size);
    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_string(fdt, "/", "model", mc->desc);
    qemu_fdt_setprop_string(fdt, "/", "compatible", WR_ARM_COMPATIBLE);
    qemu_fdt_add_subnode(fdt, "/chosen");

    machine->fdt = fdt;
}

static void wr_arm_init(MachineState *machine)
{
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    if (memory_region_size(machine->ram) > wr_memmap[WR_LO_MEM].size) {
        memory_region_init_alias(&s->mem_hi, OBJECT(machine), "ddr-ram-hi",
                                 machine->ram, wr_memmap[WR_LO_MEM].size,
                                 wr_memmap[WR_HI_MEM].size);
        memory_region_add_subregion(get_system_memory(), wr_memmap[WR_HI_MEM].base, &s->mem_hi);
    }

    memory_region_init_alias(&s->mem_lo, OBJECT(machine), "ddr-ram-low",
                             machine->ram, 0, wr_memmap[WR_LO_MEM].size);
    memory_region_add_subregion(get_system_memory(), wr_memmap[WR_LO_MEM].base, &s->mem_lo);

    fdt_create(machine);

    wr_arm_create_cpus(machine);

    wr_arm_create_gic(machine);

    wr_arm_create_timer(machine);

    wr_arm_create_clock(machine);

    wr_arm_create_uart(machine);

    wr_arm_create_virtio_transports(machine);

    s->binfo.ram_size = machine->ram_size;
    if (wr_memmap[WR_LO_MEM].size) {
        s->binfo.loader_start = wr_memmap[WR_LO_MEM].base;
    } else {
        s->binfo.loader_start = wr_memmap[WR_HI_MEM].base;
    }
    s->binfo.get_dtb = wr_arm_get_dtb;
    s->binfo.psci_conduit = QEMU_PSCI_CONDUIT_SMC;

    arm_load_kernel(&s->cpus[0], machine, &s->binfo);
}

static void wr_arm_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Wind River Generic ARM Machine";
    mc->init = wr_arm_init;
    mc->max_cpus = WR_MAX_CPUS;
    mc->default_cpus = 8;
    mc->no_cdrom = true;
    mc->default_ram_id = "ddr";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME(WR_DEFAULT_ARM_CPU);
}

static const TypeInfo wr_arm_machine_info = {
    .name = TYPE_WR_ARM_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(WrArmMachineState),
    .class_init = wr_arm_machine_class_init,
};

static void wr_arm_machine_init(void)
{
    type_register_static(&wr_arm_machine_info);
}

type_init(wr_arm_machine_init)
