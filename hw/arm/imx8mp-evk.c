/*
 * NXP i.MX 8M Plus Evaluation Kit System Emulation
 *
 * Copyright (c) 2024, Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/arm/bsa.h"
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

    struct {
        uint32_t gic;
        uint32_t osc_32k;
        uint32_t osc_24m;
        uint32_t clk_ext1;
        uint32_t clk_ext2;
        uint32_t clk_ext3;
        uint32_t clk_ext4;
        uint32_t ccm;
    } phandle;
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

static void imx8mp_fdt_add_gic(Imx8mpEvk *s,
                               const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/gic@%lx", parent,
                           fsl_imx8mp_memmap[FSL_IMX8MP_GIC_DIST].addr);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_cell(s->fdt, name, "phandle",
                          s->phandle.gic);
    qemu_fdt_setprop_cells(s->fdt, name, "interrupts",
                           GIC_FDT_IRQ_TYPE_PPI,
                           ARCH_GIC_MAINT_IRQ,
                           GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    qemu_fdt_setprop(s->fdt, name, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_DIST].addr,
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_DIST].size,
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_REDIST].addr,
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_REDIST].size);
    qemu_fdt_setprop_cell(s->fdt, name, "#interrupt-cells", 3);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "arm,gic-v3");
    g_free(name);
}

static void imx8mp_fdt_add_clock(Imx8mpEvk *s,
                                 const char *clockname,
                                 const char *clock_output,
                                 const char *parent,
                                 unsigned freq,
                                 uint32_t phandle)
{
    char *name;

    name = g_strdup_printf("%s/%s", parent, clockname);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_cell(s->fdt, name, "phandle", phandle);
    qemu_fdt_setprop_string(s->fdt, name, "clock-output-names", clock_output);
    qemu_fdt_setprop_cell(s->fdt, name, "clock-frequency", freq);
    qemu_fdt_setprop_cell(s->fdt, name, "#clock-cells", 0x0);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "fixed-clock");
    g_free(name);
}

static void imx8mp_fdt_add_ccm(Imx8mpEvk *s,
                               const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/clock-controller@%lx", parent,
                           fsl_imx8mp_memmap[FSL_IMX8MP_CCM].addr);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_cell(s->fdt, name, "phandle", s->phandle.ccm);
    qemu_fdt_setprop_cells(s->fdt, name, "clocks",
                           s->phandle.osc_32k,
                           s->phandle.osc_24m);
    qemu_fdt_setprop_cell(s->fdt, name, "#clock-cells", 0x0);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_CCM].addr,
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_CCM].size);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "fsl,imx8mp-ccm");
    g_free(name);
}

static void imx8mp_fdt_add_gpio(Imx8mpEvk *s,
                                const char *parent)
{
    int i;
    struct {
        uint64_t addr;
        uint64_t size;
        unsigned int irq_low;
        unsigned int irq_high;
    } gpio_table[FSL_IMX8MP_NUM_GPIOS] = {
        {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO1].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO1].size,
            FSL_IMX8MP_GPIO1_LOW_IRQ,
            FSL_IMX8MP_GPIO1_HIGH_IRQ
        },
        {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO2].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO2].size,
            FSL_IMX8MP_GPIO2_LOW_IRQ,
            FSL_IMX8MP_GPIO2_HIGH_IRQ
        },
        {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO3].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO3].size,
            FSL_IMX8MP_GPIO3_LOW_IRQ,
            FSL_IMX8MP_GPIO3_HIGH_IRQ
        },
        {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO4].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO4].size,
            FSL_IMX8MP_GPIO4_LOW_IRQ,
            FSL_IMX8MP_GPIO4_HIGH_IRQ
        },
        {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO5].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPIO5].size,
            FSL_IMX8MP_GPIO5_LOW_IRQ,
            FSL_IMX8MP_GPIO5_HIGH_IRQ
        },
    };

    for (i = 0; i < FSL_IMX8MP_NUM_GPIOS; i++) {
        char *name = g_strdup_printf("%s/gpio@%lx", parent,
                                     gpio_table[i].addr);
        qemu_fdt_add_subnode(s->fdt, name);
        qemu_fdt_setprop_cell(s->fdt, name, "#interrupt-cells", 2);
        qemu_fdt_setprop(s->fdt, name, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(s->fdt, name, "#gpio-cells", 2);
        qemu_fdt_setprop(s->fdt, name, "gpio-controller", NULL, 0);
        qemu_fdt_setprop_cells(s->fdt, name, "clocks",
                               s->phandle.ccm);
        qemu_fdt_setprop_cells(s->fdt, name, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI,
                               gpio_table[i].irq_low,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI,
                               GIC_FDT_IRQ_TYPE_SPI,
                               gpio_table[i].irq_high,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);
        qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                     1, gpio_table[i].addr,
                                     1, gpio_table[i].size);
        qemu_fdt_setprop_string(s->fdt, name, "compatible",
                                "fsl,imx8mp-gpio");
        g_free(name);
    }
}

static void imx8mp_fdt_add_anatop(Imx8mpEvk *s,
                                  const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/anatop@%lx", parent,
                           fsl_imx8mp_memmap[FSL_IMX8MP_ANA_PLL].addr);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_ANA_PLL].addr,
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_ANA_PLL].size);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "fsl,imx8mp-anatop");
    g_free(name);
}

static void imx8mp_fdt_add_snvs(Imx8mpEvk *s,
                                const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/snvs@%lx", parent,
                           fsl_imx8mp_memmap[FSL_IMX8MP_SNVS_HP].addr);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_SNVS_HP].addr,
                                 1, fsl_imx8mp_memmap[FSL_IMX8MP_SNVS_HP].size);
    qemu_fdt_setprop_string(s->fdt, name, "compatible",
                            "fsl,sec-v4.0-mon\0syscon\0simple-mfd");
    g_free(name);
}

static void imx8mp_fdt_add_soc(Imx8mpEvk *s,
                               const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/soc@0", parent);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "ranges",
                                 2, 0,
                                 2, 0x3e000000);
    qemu_fdt_setprop_cell(s->fdt, name, "#size-cells", 0x1);
    qemu_fdt_setprop_cell(s->fdt, name, "#address-cells", 0x1);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "simple-bus");

    imx8mp_fdt_add_ccm(s, name);
    imx8mp_fdt_add_gpio(s, name);
    imx8mp_fdt_add_anatop(s, name);
    imx8mp_fdt_add_snvs(s, name);

    g_free(name);
}

static void imx8mp_fdt_add_gpt_timer(Imx8mpEvk *s,
                                     const char *parent)
{
    char *name;

    name = g_strdup_printf("%s/timer@%lx", parent,
                           fsl_imx8mp_memmap[FSL_IMX8MP_GPT1].addr);
    qemu_fdt_add_subnode(s->fdt, name);
    qemu_fdt_setprop_cell(s->fdt, name, "interrupt-parent",
                          s->phandle.gic);
    qemu_fdt_setprop_cells(s->fdt, name, "interrupts",
                           GIC_FDT_IRQ_TYPE_SPI,
                           FSL_IMX8MP_GPT1_IRQ,
                           GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    qemu_fdt_setprop_cells(s->fdt, name, "clocks",
                           s->phandle.ccm);
    qemu_fdt_setprop_sized_cells(s->fdt, name, "reg",
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GPT1].addr,
                                 2, fsl_imx8mp_memmap[FSL_IMX8MP_GPT1].size);
    qemu_fdt_setprop_string(s->fdt, name, "compatible", "fsl,imx-gpt");
    g_free(name);
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

    /* Allocate phandles */
    s->phandle.gic = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.osc_32k = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.osc_24m = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.clk_ext1 = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.clk_ext2 = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.clk_ext3 = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.clk_ext4 = qemu_fdt_alloc_phandle(s->fdt);
    s->phandle.ccm = qemu_fdt_alloc_phandle(s->fdt);

    /* Device Tree Root */
    root = g_strdup_printf("/");

    /* Header */
    qemu_fdt_setprop_cell(s->fdt, root, "interrupt-parent", s->phandle.gic);
    qemu_fdt_setprop_cell(s->fdt, root, "#size-cells", 0x2);
    qemu_fdt_setprop_cell(s->fdt, root, "#address-cells", 0x2);
    qemu_fdt_setprop_string(s->fdt, root, "model", mc->desc);
    qemu_fdt_setprop_string(s->fdt, root, "compatible", "fsl,imx8mp");

    /* Chosen node */
    qemu_fdt_add_subnode(s->fdt, "/chosen");

    imx8mp_fdt_add_cpus(s, machine, root);
    imx8mp_fdt_add_gic(s, root);
    imx8mp_fdt_add_clock(s, "clock-osc-32k", "osc_32k", root,
                         0x8000, s->phandle.osc_32k);
    imx8mp_fdt_add_clock(s, "clock-osc-24m", "osc_24m", root,
                         0x16e3600, s->phandle.osc_24m);
    imx8mp_fdt_add_clock(s, "clock-ext1", "clk_ext1", root,
                         0x7ed6b40, s->phandle.clk_ext1);
    imx8mp_fdt_add_clock(s, "clock-ext2", "clk_ext2", root,
                         0x7ed6b40, s->phandle.clk_ext2);
    imx8mp_fdt_add_clock(s, "clock-ext3", "clk_ext3", root,
                         0x7ed6b40, s->phandle.clk_ext3);
    imx8mp_fdt_add_clock(s, "clock-ext4", "clk_ext4", root,
                         0x7ed6b40, s->phandle.clk_ext4);
    imx8mp_fdt_add_soc(s, root);
    imx8mp_fdt_add_gpt_timer(s, root);

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
