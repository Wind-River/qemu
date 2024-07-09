/*
 * Simple arm machine emulation
 *
 * Copyright (C) 2024 Wind River Systems, Inc.
 *
 * Written-by: Nelson Ho <nelson.ho@windriver.com>
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
    memory_region_add_subregion(s->mr, MM_UART0, mr);

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
                                 MM_UART0, 2, 0x1000);
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
                                ARM_CPU_TYPE_NAME("cortex-a53"));
        obj = OBJECT(&s->cpus[i]);
        if (i) {
            /* start APs in powered down state */
            object_property_set_bool(obj, "start-powered-off", true,
                                     &error_abort);
        }

        if (!s->secure) {
            object_property_set_bool(obj, "has_el3", false, NULL);
        }

        if (!s->virt && object_property_find(obj, "has_el2")) {
            object_property_set_bool(obj, "has_el2", false, NULL);
        }

        object_property_set_int(obj, "core-count", 1, &error_abort); //TODO
        object_property_set_link(obj, "memory", OBJECT(s->mr),
                                 &error_abort);
        qdev_realize(DEVICE(obj), NULL, &error_fatal);
    }

    qdev_realize(DEVICE(&s->cpu_cluster), NULL, &error_fatal);

    /* add FDT nodes for CPUs in reverse order */
    qemu_fdt_add_subnode(machine->fdt, "/cpus");
    qemu_fdt_setprop_cell(machine->fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(machine->fdt, "/cpus", "#address-cells", 1);

    for (i = machine->smp.cpus - 1; i >= 0; i--) {
        char *name = g_strdup_printf("/cpus/cpu%d", i);
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(i));

        qemu_fdt_add_subnode(machine->fdt, name);
        qemu_fdt_setprop_cell(machine->fdt, name, "reg", armcpu->mp_affinity);
        if (s->psci_conduit != QEMU_PSCI_CONDUIT_DISABLED) {
            qemu_fdt_setprop_string(machine->fdt, name, "enable-method", "psci");
        }
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

    s->psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    s->mr = get_system_memory();

    fdt_create(machine);

    wr_arm_create_cpus(machine);

    memory_region_add_subregion(s->mr, WR_PHYSMEM_BASE, machine->ram);

    s->binfo.ram_size = machine->ram_size;
}
    wr_arm_create_uart(machine);

static void wr_arm_machine_instance_init(Object *obj)
{
    WrArmMachineState *s = WR_ARM_MACHINE(obj);

    s->secure = false;
    s->virt = false;
}

static void wr_arm_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Wind River Generic ARM Machine";
    mc->init = wr_arm_init;
    mc->max_cpus = 8;
    mc->default_cpus = 8;
    mc->no_cdrom = true;
    mc->default_ram_id = "ddr";
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a53");
}

static const TypeInfo wr_arm_machine_info = {
    .name = TYPE_WR_ARM_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(WrArmMachineState),
    .class_init = wr_arm_machine_class_init,
    .instance_init = wr_arm_machine_instance_init,
};

static void wr_arm_machine_init(void)
{
    type_register_static(&wr_arm_machine_info);
}

type_init(wr_arm_machine_init)
