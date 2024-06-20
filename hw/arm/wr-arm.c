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


static void wr_arm_init(MachineState *machine)
{
    WrArmMachineState *s = WR_ARM_MACHINE(machine);

    s->psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    s->mr = get_system_memory();
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
