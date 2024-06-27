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

static void wr_arm_create_cpus(MachineState *machine)
{
    int i;
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    /* realize the cpu cluster and cpus */
    object_initialize_child(OBJECT(s), "cpu-cluster", &s->cpu_cluster,
                            TYPE_CPU_CLUSTER);

    qdev_prop_set_uint32(DEVICE(&s->cpu_cluster), "cluster-id", 0);
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
