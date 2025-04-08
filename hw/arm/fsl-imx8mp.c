/*
 * i.MX 8M Plus SoC Implementation
 *
 * Based on hw/arm/fsl-imx6.c
 *
 * Copyright (c) 2024, Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/arm/bsa.h"
#include "hw/arm/fsl-imx8mp.h"
#include "hw/intc/arm_gicv3.h"
#include "hw/misc/unimp.h"
#include "hw/boards.h"
#include "system/system.h"
#include "target/arm/cpu-qom.h"
#include "qapi/error.h"
#include "qobject/qlist.h"

static void fsl_imx8mp_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    FslImx8mpState *s = FSL_IMX8MP(obj);
    int i;

    for (i = 0; i < MIN(ms->smp.cpus, FSL_IMX8MP_NUM_CPUS); i++) {
        g_autofree char *name = g_strdup_printf("cpu%d", i);
        object_initialize_child(obj, name, &s->cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GICV3);

    object_initialize_child(obj, "ccm", &s->ccm, TYPE_IMX8MP_CCM);

    object_initialize_child(obj, "analog", &s->analog, TYPE_IMX8MP_ANALOG);

    object_initialize_child(obj, "snvs", &s->snvs, TYPE_IMX7_SNVS);

    for (i = 0; i < FSL_IMX8MP_NUM_UARTS; i++) {
        g_autofree char *name = g_strdup_printf("uart%d", i + 1);
        object_initialize_child(obj, name, &s->uart[i], TYPE_IMX_SERIAL);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_GPTS; i++) {
        g_autofree char *name = g_strdup_printf("gpt%d", i + 1);
        object_initialize_child(obj, name, &s->gpt[i], TYPE_IMX8MP_GPT);
    }
    object_initialize_child(obj, "gpt5-gpt6-irq", &s->gpt5_gpt6_irq,
                            TYPE_OR_IRQ);

    for (i = 0; i < FSL_IMX8MP_NUM_I2CS; i++) {
        g_autofree char *name = g_strdup_printf("i2c%d", i + 1);
        object_initialize_child(obj, name, &s->i2c[i], TYPE_IMX_I2C);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_GPIOS; i++) {
        g_autofree char *name = g_strdup_printf("gpio%d", i + 1);
        object_initialize_child(obj, name, &s->gpio[i], TYPE_IMX_GPIO);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_USDHCS; i++) {
        g_autofree char *name = g_strdup_printf("usdhc%d", i + 1);
        object_initialize_child(obj, name, &s->usdhc[i], TYPE_IMX_USDHC);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_USBS; i++) {
        g_autofree char *name = g_strdup_printf("usb%d", i);
        object_initialize_child(obj, name, &s->usb[i], TYPE_USB_DWC3);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_ECSPIS; i++) {
        g_autofree char *name = g_strdup_printf("spi%d", i + 1);
        object_initialize_child(obj, name, &s->spi[i], TYPE_IMX_SPI);
    }

    for (i = 0; i < FSL_IMX8MP_NUM_WDTS; i++) {
        g_autofree char *name = g_strdup_printf("wdt%d", i);
        object_initialize_child(obj, name, &s->wdt[i], TYPE_IMX2_WDT);
    }

    object_initialize_child(obj, "eth0", &s->enet, TYPE_IMX_ENET);

    object_initialize_child(obj, "pcie", &s->pcie, TYPE_DESIGNWARE_PCIE_HOST);
    object_initialize_child(obj, "pcie_phy", &s->pcie_phy,
                            TYPE_FSL_IMX8M_PCIE_PHY);
}

static void fsl_imx8mp_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    FslImx8mpState *s = FSL_IMX8MP(dev);
    DeviceState *gicdev = DEVICE(&s->gic);
    int i;

    if (ms->smp.cpus > FSL_IMX8MP_NUM_CPUS) {
        error_setg(errp, "%s: Only %d CPUs are supported (%d requested)",
                   TYPE_FSL_IMX8MP, FSL_IMX8MP_NUM_CPUS, ms->smp.cpus);
        return;
    }

    /* CPUs */
    for (i = 0; i < ms->smp.cpus; i++) {
        /* On uniprocessor, the CBAR is set to 0 */
        if (ms->smp.cpus > 1) {
            object_property_set_int(OBJECT(&s->cpu[i]), "reset-cbar",
                                    fsl_imx8mp_memmap[FSL_IMX8MP_GIC_DIST].addr,
                                    &error_abort);
        }

        /*
         * CNTFID0 base frequency in Hz of system counter
         */
        object_property_set_int(OBJECT(&s->cpu[i]), "cntfrq", 8000000,
                                &error_abort);

        if (i) {
            /*
             * Secondary CPUs start in powered-down state (and can be
             * powered up via the SRC system reset controller)
             */
            object_property_set_bool(OBJECT(&s->cpu[i]), "start-powered-off",
                                     true, &error_abort);
        }

        if (!qdev_realize(DEVICE(&s->cpu[i]), NULL, errp)) {
            return;
        }
    }

    /* GIC */
    {
        SysBusDevice *gicsbd = SYS_BUS_DEVICE(&s->gic);
        QList *redist_region_count;

        qdev_prop_set_uint32(gicdev, "num-cpu", ms->smp.cpus);
        qdev_prop_set_uint32(gicdev, "num-irq",
                             FSL_IMX8MP_NUM_IRQS + GIC_INTERNAL);
        redist_region_count = qlist_new();
        qlist_append_int(redist_region_count, ms->smp.cpus);
        qdev_prop_set_array(gicdev, "redist-region-count", redist_region_count);
        object_property_set_link(OBJECT(&s->gic), "sysmem",
                                 OBJECT(get_system_memory()), &error_fatal);
        if (!sysbus_realize(gicsbd, errp)) {
            return;
        }
        sysbus_mmio_map(gicsbd, 0, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_DIST].addr);
        sysbus_mmio_map(gicsbd, 1, fsl_imx8mp_memmap[FSL_IMX8MP_GIC_REDIST].addr);

        /*
         * Wire the outputs from each CPU's generic timer and the GICv3
         * maintenance interrupt signal to the appropriate GIC PPI inputs, and
         * the GIC's IRQ/FIQ interrupt outputs to the CPU's inputs.
         */
        for (i = 0; i < ms->smp.cpus; i++) {
            DeviceState *cpudev = DEVICE(&s->cpu[i]);
            int intidbase = FSL_IMX8MP_NUM_IRQS + i * GIC_INTERNAL;
            qemu_irq irq;

            /*
             * Mapping from the output timer irq lines from the CPU to the
             * GIC PPI inputs.
             */
            static const int timer_irqs[] = {
                [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
                [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
                [GTIMER_HYP]  = ARCH_TIMER_NS_EL2_IRQ,
                [GTIMER_SEC]  = ARCH_TIMER_S_EL1_IRQ,
            };

            for (int j = 0; j < ARRAY_SIZE(timer_irqs); j++) {
                irq = qdev_get_gpio_in(gicdev, intidbase + timer_irqs[j]);
                qdev_connect_gpio_out(cpudev, j, irq);
            }

            irq = qdev_get_gpio_in(gicdev, intidbase + ARCH_GIC_MAINT_IRQ);
            qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt",
                                        0, irq);

            irq = qdev_get_gpio_in(gicdev, intidbase + VIRTUAL_PMU_IRQ);
            qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0, irq);

            sysbus_connect_irq(gicsbd, i,
                               qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
            sysbus_connect_irq(gicsbd, i + ms->smp.cpus,
                               qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        }
    }

    /* CCM */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->ccm), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_CCM].addr);

    /* Analog */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->analog), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->analog), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_ANA_PLL].addr);

    /* UARTs */
    for (i = 0; i < FSL_IMX8MP_NUM_UARTS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } serial_table[FSL_IMX8MP_NUM_UARTS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_UART1].addr, FSL_IMX8MP_UART1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_UART2].addr, FSL_IMX8MP_UART2_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_UART3].addr, FSL_IMX8MP_UART3_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_UART4].addr, FSL_IMX8MP_UART4_IRQ },
        };

        qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, serial_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0,
                           qdev_get_gpio_in(gicdev, serial_table[i].irq));
    }

    /* GPTs */
    object_property_set_int(OBJECT(&s->gpt5_gpt6_irq), "num-lines", 2,
                            &error_abort);
    if (!qdev_realize(DEVICE(&s->gpt5_gpt6_irq), NULL, errp)) {
        return;
    }

    qdev_connect_gpio_out(DEVICE(&s->gpt5_gpt6_irq), 0,
                          qdev_get_gpio_in(gicdev, FSL_IMX8MP_GPT5_GPT6_IRQ));

    for (i = 0; i < FSL_IMX8MP_NUM_GPTS; i++) {
        hwaddr gpt_addrs[FSL_IMX8MP_NUM_GPTS] = {
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT1].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT2].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT3].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT4].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT5].addr,
            fsl_imx8mp_memmap[FSL_IMX8MP_GPT6].addr,
        };

        s->gpt[i].ccm = IMX_CCM(&s->ccm);

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpt[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpt[i]), 0, gpt_addrs[i]);

        if (i < FSL_IMX8MP_NUM_GPTS - 2) {
            static const unsigned int gpt_irqs[FSL_IMX8MP_NUM_GPTS - 2] = {
                FSL_IMX8MP_GPT1_IRQ,
                FSL_IMX8MP_GPT2_IRQ,
                FSL_IMX8MP_GPT3_IRQ,
                FSL_IMX8MP_GPT4_IRQ,
            };

            sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpt[i]), 0,
                               qdev_get_gpio_in(gicdev, gpt_irqs[i]));
        } else {
            int irq = i - FSL_IMX8MP_NUM_GPTS + 2;

            sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpt[i]), 0,
                               qdev_get_gpio_in(DEVICE(&s->gpt5_gpt6_irq), irq));
        }
    }

    /* I2Cs */
    for (i = 0; i < FSL_IMX8MP_NUM_I2CS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } i2c_table[FSL_IMX8MP_NUM_I2CS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C1].addr, FSL_IMX8MP_I2C1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C2].addr, FSL_IMX8MP_I2C2_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C3].addr, FSL_IMX8MP_I2C3_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C4].addr, FSL_IMX8MP_I2C4_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C5].addr, FSL_IMX8MP_I2C5_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_I2C6].addr, FSL_IMX8MP_I2C6_IRQ },
        };

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->i2c[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c[i]), 0, i2c_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c[i]), 0,
                           qdev_get_gpio_in(gicdev, i2c_table[i].irq));
    }

    /* GPIOs */
    for (i = 0; i < FSL_IMX8MP_NUM_GPIOS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq_low;
            unsigned int irq_high;
        } gpio_table[FSL_IMX8MP_NUM_GPIOS] = {
            {
                fsl_imx8mp_memmap[FSL_IMX8MP_GPIO1].addr,
                FSL_IMX8MP_GPIO1_LOW_IRQ,
                FSL_IMX8MP_GPIO1_HIGH_IRQ
            },
            {
                fsl_imx8mp_memmap[FSL_IMX8MP_GPIO2].addr,
                FSL_IMX8MP_GPIO2_LOW_IRQ,
                FSL_IMX8MP_GPIO2_HIGH_IRQ
            },
            {
                fsl_imx8mp_memmap[FSL_IMX8MP_GPIO3].addr,
                FSL_IMX8MP_GPIO3_LOW_IRQ,
                FSL_IMX8MP_GPIO3_HIGH_IRQ
            },
            {
                fsl_imx8mp_memmap[FSL_IMX8MP_GPIO4].addr,
                FSL_IMX8MP_GPIO4_LOW_IRQ,
                FSL_IMX8MP_GPIO4_HIGH_IRQ
            },
            {
                fsl_imx8mp_memmap[FSL_IMX8MP_GPIO5].addr,
                FSL_IMX8MP_GPIO5_LOW_IRQ,
                FSL_IMX8MP_GPIO5_HIGH_IRQ
            },
        };

        object_property_set_bool(OBJECT(&s->gpio[i]), "has-edge-sel", true,
                                 &error_abort);
        object_property_set_bool(OBJECT(&s->gpio[i]), "has-upper-pin-irq",
                                 true, &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0, gpio_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 0,
                           qdev_get_gpio_in(gicdev, gpio_table[i].irq_low));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 1,
                           qdev_get_gpio_in(gicdev, gpio_table[i].irq_high));
    }

    /* USDHCs */
    for (i = 0; i < FSL_IMX8MP_NUM_USDHCS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } usdhc_table[FSL_IMX8MP_NUM_USDHCS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_USDHC1].addr, FSL_IMX8MP_USDHC1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_USDHC2].addr, FSL_IMX8MP_USDHC2_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_USDHC3].addr, FSL_IMX8MP_USDHC3_IRQ },
        };

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usdhc[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usdhc[i]), 0, usdhc_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->usdhc[i]), 0,
                           qdev_get_gpio_in(gicdev, usdhc_table[i].irq));
    }

    /* USBs */
    for (i = 0; i < FSL_IMX8MP_NUM_USBS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } usb_table[FSL_IMX8MP_NUM_USBS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_USB1].addr, FSL_IMX8MP_USB1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_USB2].addr, FSL_IMX8MP_USB2_IRQ },
        };

        qdev_prop_set_uint32(DEVICE(&s->usb[i].sysbus_xhci), "p2", 1);
        qdev_prop_set_uint32(DEVICE(&s->usb[i].sysbus_xhci), "p3", 1);
        qdev_prop_set_uint32(DEVICE(&s->usb[i].sysbus_xhci), "slots", 2);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usb[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usb[i]), 0, usb_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->usb[i].sysbus_xhci), 0,
                           qdev_get_gpio_in(gicdev, usb_table[i].irq));
    }

    /* ECSPIs */
    for (i = 0; i < FSL_IMX8MP_NUM_ECSPIS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } spi_table[FSL_IMX8MP_NUM_ECSPIS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_ECSPI1].addr, FSL_IMX8MP_ECSPI1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_ECSPI2].addr, FSL_IMX8MP_ECSPI2_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_ECSPI3].addr, FSL_IMX8MP_ECSPI3_IRQ },
        };

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 0, spi_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->spi[i]), 0,
                           qdev_get_gpio_in(gicdev, spi_table[i].irq));
    }

    /* ENET1 */
    object_property_set_uint(OBJECT(&s->enet), "phy-num", s->phy_num,
                             &error_abort);
    object_property_set_uint(OBJECT(&s->enet), "tx-ring-num", 3, &error_abort);
    qemu_configure_nic_device(DEVICE(&s->enet), true, NULL);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->enet), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->enet), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_ENET1].addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->enet), 0,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_ENET1_MAC_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->enet), 1,
                       qdev_get_gpio_in(gicdev, FSL_IMX6_ENET1_MAC_1588_IRQ));

    /* SNVS */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->snvs), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->snvs), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_SNVS_HP].addr);

    /* Watchdogs */
    for (i = 0; i < FSL_IMX8MP_NUM_WDTS; i++) {
        struct {
            hwaddr addr;
            unsigned int irq;
        } wdog_table[FSL_IMX8MP_NUM_WDTS] = {
            { fsl_imx8mp_memmap[FSL_IMX8MP_WDOG1].addr, FSL_IMX8MP_WDOG1_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_WDOG2].addr, FSL_IMX8MP_WDOG2_IRQ },
            { fsl_imx8mp_memmap[FSL_IMX8MP_WDOG3].addr, FSL_IMX8MP_WDOG3_IRQ },
        };

        object_property_set_bool(OBJECT(&s->wdt[i]), "pretimeout-support",
                                 true, &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->wdt[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->wdt[i]), 0, wdog_table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->wdt[i]), 0,
                           qdev_get_gpio_in(gicdev, wdog_table[i].irq));
    }

    /* PCIe */
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->pcie), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->pcie), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_PCIE1].addr);

    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 0,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_PCI_INTA_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 1,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_PCI_INTB_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 2,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_PCI_INTC_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 3,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_PCI_INTD_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->pcie), 4,
                       qdev_get_gpio_in(gicdev, FSL_IMX8MP_PCI_MSI_IRQ));

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->pcie_phy), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->pcie_phy), 0,
                    fsl_imx8mp_memmap[FSL_IMX8MP_PCIE_PHY1].addr);

    /* On-Chip RAM */
    if (!memory_region_init_ram(&s->ocram, NULL, "imx8mp.ocram",
                                fsl_imx8mp_memmap[FSL_IMX8MP_OCRAM].size,
                                errp)) {
        return;
    }
    memory_region_add_subregion(get_system_memory(),
                                fsl_imx8mp_memmap[FSL_IMX8MP_OCRAM].addr,
                                &s->ocram);

    /* Unimplemented devices */
    for (i = 0; i < ARRAY_SIZE(fsl_imx8mp_memmap); i++) {
        switch (i) {
        case FSL_IMX8MP_ANA_PLL:
        case FSL_IMX8MP_CCM:
        case FSL_IMX8MP_GIC_DIST:
        case FSL_IMX8MP_GIC_REDIST:
        case FSL_IMX8MP_GPIO1 ... FSL_IMX8MP_GPIO5:
        case FSL_IMX8MP_ECSPI1 ... FSL_IMX8MP_ECSPI3:
        case FSL_IMX8MP_ENET1:
        case FSL_IMX8MP_I2C1 ... FSL_IMX8MP_I2C6:
        case FSL_IMX8MP_OCRAM:
        case FSL_IMX8MP_PCIE1:
        case FSL_IMX8MP_PCIE_PHY1:
        case FSL_IMX8MP_RAM:
        case FSL_IMX8MP_SNVS_HP:
        case FSL_IMX8MP_UART1 ... FSL_IMX8MP_UART4:
        case FSL_IMX8MP_USB1 ... FSL_IMX8MP_USB2:
        case FSL_IMX8MP_USDHC1 ... FSL_IMX8MP_USDHC3:
        case FSL_IMX8MP_WDOG1 ... FSL_IMX8MP_WDOG3:
            /* device implemented and treated above */
            break;

        default:
            create_unimplemented_device(fsl_imx8mp_memmap[i].name,
                                        fsl_imx8mp_memmap[i].addr,
                                        fsl_imx8mp_memmap[i].size);
            break;
        }
    }
}

static const Property fsl_imx8mp_properties[] = {
    DEFINE_PROP_UINT32("fec1-phy-num", FslImx8mpState, phy_num, 0),
    DEFINE_PROP_BOOL("fec1-phy-connected", FslImx8mpState, phy_connected, true),
};

static void fsl_imx8mp_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_props(dc, fsl_imx8mp_properties);
    dc->realize = fsl_imx8mp_realize;

    dc->desc = "i.MX 8M Plus SoC";
}

static const TypeInfo fsl_imx8mp_types[] = {
    {
        .name = TYPE_FSL_IMX8MP,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpState),
        .instance_init = fsl_imx8mp_init,
        .class_init = fsl_imx8mp_class_init,
    },
};

DEFINE_TYPES(fsl_imx8mp_types)
