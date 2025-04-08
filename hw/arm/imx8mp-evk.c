/*
 * NXP i.MX 8M Plus Evaluation Kit System Emulation
 *
 * Copyright (c) 2024, Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/arm/boot.h"
#include "hw/arm/fsl-imx8mp.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/arm/fdt.h"
#include "system/device_tree.h"
#include "system/qtest.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include <libfdt.h>

#define TYPE_IMX8MP_EVK_MACHINE MACHINE_TYPE_NAME("imx8mp-evk")
OBJECT_DECLARE_SIMPLE_TYPE(Imx8mpEvk, IMX8MP_EVK_MACHINE)

struct Imx8mpEvk {
    MachineState parent_obj;

    FslImx8mpState soc;

    struct arm_boot_info binfo;

    void *fdt;
};


static void imx8mp_evk_modify_dtb(const struct arm_boot_info *info, void *fdt)
{
    int i, offset;

    /* Temporarily disable following nodes until they are implemented */
    const char *nodes_to_remove[] = {
        "nxp,imx8mp-fspi",
    };

    for (i = 0; i < ARRAY_SIZE(nodes_to_remove); i++) {
        const char *dev_str = nodes_to_remove[i];

        offset = fdt_node_offset_by_compatible(fdt, -1, dev_str);
        while (offset >= 0) {
            fdt_nop_node(fdt, offset);
            offset = fdt_node_offset_by_compatible(fdt, offset, dev_str);
        }
    }

    /* Remove cpu-idle-states property from CPU nodes */
    offset = fdt_node_offset_by_compatible(fdt, -1, "arm,cortex-a53");
    while (offset >= 0) {
        fdt_nop_property(fdt, offset, "cpu-idle-states");
        offset = fdt_node_offset_by_compatible(fdt, offset, "arm,cortex-a53");
    }
}

static void *imx8mp_evk_get_dtb(const struct arm_boot_info *binfo)
{
    const Imx8mpEvk *s = container_of(binfo, Imx8mpEvk, binfo);

    return s->fdt;
}

static void imx8mp_fdt_add_cpus(Imx8mpEvk *s,
                                MachineState *machine,
                                const char *parent)
{
    int i;
    char *cpus_nodename = g_strdup_printf("%s/cpus", parent);

    qemu_fdt_add_subnode(s->fdt, cpus_nodename);
    qemu_fdt_setprop_cell(s->fdt, cpus_nodename,
                          "#size-cells", 0x0);
    qemu_fdt_setprop_cell(s->fdt, cpus_nodename,
                          "#address-cells", 1);

    for (i = machine->smp.cpus - 1; i >= 0; i--) {
        char *name = g_strdup_printf("%s/cpu@%d", cpus_nodename, i);
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(i));

        qemu_fdt_add_subnode(s->fdt, name);
        qemu_fdt_setprop_cell(s->fdt, name, "reg",
                              arm_cpu_mp_affinity(armcpu));
        if (s->binfo.psci_conduit != QEMU_PSCI_CONDUIT_DISABLED) {
            qemu_fdt_setprop_string(s->fdt, name,
                                    "enable-method", "psci");
        }
        qemu_fdt_setprop_string(s->fdt, name, "device_type", "cpu");
        qemu_fdt_setprop_string(s->fdt, name, "compatible",
                                armcpu->dtb_compatible);
        g_free(name);
    }
    g_free(cpus_nodename);
}

static void imx8mp_fdt_create(Imx8mpEvk *s,
                              MachineState *machine)
{
    MachineClass *mc = MACHINE_GET_CLASS(s);
    char *root;
    int fdt_size;

    s->fdt = create_device_tree(&fdt_size);
    if (!s->fdt) {
        error_report("imx8mp_fdt_create failed");
        exit(1);
    }

    /* Device Tree Root */
    root = g_strdup_printf("/");

    /* Header */
    qemu_fdt_setprop_cell(s->fdt, root, "#size-cells", 0x2);
    qemu_fdt_setprop_cell(s->fdt, root, "#address-cells", 0x2);
    qemu_fdt_setprop_string(s->fdt, root, "model", mc->desc);
    qemu_fdt_setprop_string(s->fdt, root, "compatible", "fsl,imx8mp");

    /* Chosen node */
    qemu_fdt_add_subnode(s->fdt, "/chosen");

    imx8mp_fdt_add_cpus(s, machine, root);

    g_free(root);
}

static void imx8mp_evk_init(MachineState *machine)
{
    Imx8mpEvk *s = IMX8MP_EVK_MACHINE(machine);

    if (machine->ram_size > FSL_IMX8MP_RAM_SIZE_MAX) {
        error_report("RAM size " RAM_ADDR_FMT " above max supported (%08" PRIx64 ")",
                     machine->ram_size, FSL_IMX8MP_RAM_SIZE_MAX);
        exit(1);
    }

    s->binfo.loader_start = FSL_IMX8MP_RAM_START;
    s->binfo.board_id = -1;
    s->binfo.ram_size = machine->ram_size;
    s->binfo.psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    s->binfo.modify_dtb = imx8mp_evk_modify_dtb;
    s->binfo.get_dtb = imx8mp_evk_get_dtb;

    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_FSL_IMX8MP);
    object_property_set_uint(OBJECT(&s->soc), "fec1-phy-num", 1, &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(&s->soc), &error_fatal);

    memory_region_add_subregion(get_system_memory(), FSL_IMX8MP_RAM_START,
                                machine->ram);

    for (int i = 0; i < FSL_IMX8MP_NUM_USDHCS; i++) {
        BusState *bus;
        DeviceState *carddev;
        BlockBackend *blk;
        DriveInfo *di = drive_get(IF_SD, i, 0);

        if (!di) {
            continue;
        }

        blk = blk_by_legacy_dinfo(di);
        bus = qdev_get_child_bus(DEVICE(&s->soc.usdhc[i]), "sd-bus");
        carddev = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
        qdev_realize_and_unref(carddev, bus, &error_fatal);
    }

    imx8mp_fdt_create(s, machine);

    if (!qtest_enabled()) {
        arm_load_kernel(&s->soc.cpu[0], machine, &s->binfo);
    }
}

static void imx8mp_evk_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "NXP i.MX 8M Plus EVK Board";
    mc->init = imx8mp_evk_init;
    mc->max_cpus = FSL_IMX8MP_NUM_CPUS;
    mc->default_ram_id = "imx8mp-evk.ram";
}

static const TypeInfo imx8mp_evk_machine_init_typeinfo = {
    .name = TYPE_IMX8MP_EVK_MACHINE,
    .parent = TYPE_MACHINE,
    .class_init = imx8mp_evk_machine_class_init,
    .instance_size = sizeof(Imx8mpEvk),
};

static void imx8mp_evk_machine_init_register_types(void)
{
    type_register_static(&imx8mp_evk_machine_init_typeinfo);
}

type_init(imx8mp_evk_machine_init_register_types)
