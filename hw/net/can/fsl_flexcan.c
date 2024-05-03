/*
 * QEMU model of Freescale FlexCAN Controller
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
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "hw/net/fsl_flexcan.h"

REG32(MODULE_CONFIGURATION_REGISTER, 0X0)
    FIELD(MODULE_CONFIGURATION_REGISTER, MDIS,      31, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, FRZ,       30, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, RFEN,      29, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, HALT,      28, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, NOTRDY,    27, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, SOFTRST,   25, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, FRZACK,    24, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, WRNEN,     21, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, LPMACK,    20, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, SRXDIS,    17, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, IRMQ,      16, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, DMA,       15, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, LPRIOEN,   13, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, AEN,       12, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, FDEN,      11, 1)
    FIELD(MODULE_CONFIGURATION_REGISTER, IDAM,      8,  2)
    FIELD(MODULE_CONFIGURATION_REGISTER, MAXMB,     0,  7)
REG32(CONTROL_1_REGISTER, 0x4)
    FIELD(CONTROL_1_REGISTER, PRESDIV,  24, 8)
    FIELD(CONTROL_1_REGISTER, RJW,      22, 2)
    FIELD(CONTROL_1_REGISTER, PSEG1,    19, 3)
    FIELD(CONTROL_1_REGISTER, PSEG2,    16, 3)
    FIELD(CONTROL_1_REGISTER, BOFFMSK,  15, 1)
    FIELD(CONTROL_1_REGISTER, ERRMSK,   14, 1)
    FIELD(CONTROL_1_REGISTER, LPB,      12, 1)
    FIELD(CONTROL_1_REGISTER, TWRNMSK,  11, 1)
    FIELD(CONTROL_1_REGISTER, RWRNMSK,  10, 1)
    FIELD(CONTROL_1_REGISTER, SMP,      7,  1)
    FIELD(CONTROL_1_REGISTER, BOFFREC,  6,  1)
    FIELD(CONTROL_1_REGISTER, TSYN,     5,  1)
    FIELD(CONTROL_1_REGISTER, LBUF,     4,  1)
    FIELD(CONTROL_1_REGISTER, LOM,      3,  1)
    FIELD(CONTROL_1_REGISTER, PROPSEG,  0,  3)
REG32(RX_MAILBOXES_GLOBAL_MASK_REGISTER, 0x10)
    FIELD(RX_MAILBOXES_GLOBAL_MASK_REGISTER, MG, 0, 32)
REG32(RX_14_MASK_REGISTER, 0x14)
    FIELD(RX_14_MASK_REGISTER, RX14M, 0, 32)
REG32(RX_15_MASK_REGISTER, 0x18)
    FIELD(RX_15_MASK_REGISTER, RX15M, 0, 32)
REG32(INTERRUPT_MASKS_2_REGISTER, 0x24)
    FIELD(INTERRUPT_MASKS_2_REGISTER, BUF63TO32M,   0, 32)
REG32(INTERRUPT_MASKS_1_REGISTER, 0x28)
    FIELD(INTERRUPT_MASKS_1_REGISTER, BUF31TO0M,    0, 32)
REG32(CONTROL_2_REGISTER, 0x34)
    FIELD(CONTROL_2_REGISTER, ERRMSK_FAST,  31, 1)
    FIELD(CONTROL_2_REGISTER, BOFFDONEMSK,  30, 1)
    FIELD(CONTROL_2_REGISTER, ECRWRE,       29, 1)
    FIELD(CONTROL_2_REGISTER, WRMFRZ,       28, 1)
    FIELD(CONTROL_2_REGISTER, RFFN,         24, 4)
    FIELD(CONTROL_2_REGISTER, TASD,         19, 5)
    FIELD(CONTROL_2_REGISTER, MRP,          18, 1)
    FIELD(CONTROL_2_REGISTER, RRS,          17, 1)
    FIELD(CONTROL_2_REGISTER, EACEN,        16, 1)
    FIELD(CONTROL_2_REGISTER, TIMER_SRC,    15, 1)
    FIELD(CONTROL_2_REGISTER, PREXCEN,      14, 1)
    FIELD(CONTROL_2_REGISTER, BTE,          13, 1)
    FIELD(CONTROL_2_REGISTER, ISOCANFDEN,   12, 1)
    FIELD(CONTROL_2_REGISTER, EDFLTDIS,     11, 1)
    FIELD(CONTROL_2_REGISTER, MBTSBASE,     8,  2)
    FIELD(CONTROL_2_REGISTER, TSTAMPCAP,    6,  2)
REG32(MEMORY_ERROR_CONTROL_REGISTER, 0xAE0)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, ECRWRDIS,  31, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, HAERRIE,   15, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, FAERRIE,   14, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, EXTERRIE,  13, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, RERRDIS,   9,  1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, ECCDIS,    8,  1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, NCEFAFRZ,  7,  1)
REG32(CAN_FD_CONTROL_REGISTER, 0xC00)
    FIELD(CAN_FD_CONTROL_REGISTER, FDRATE,  31, 1)
    FIELD(CAN_FD_CONTROL_REGISTER, MBDSR3,  25, 2)
    FIELD(CAN_FD_CONTROL_REGISTER, MBDSR2,  22, 2)
    FIELD(CAN_FD_CONTROL_REGISTER, MBDSR1,  19, 2)
    FIELD(CAN_FD_CONTROL_REGISTER, MBDSR0,  16, 2)
    FIELD(CAN_FD_CONTROL_REGISTER, TDCEN,   15, 1)
    FIELD(CAN_FD_CONTROL_REGISTER, TDCFAIL, 14, 1)
    FIELD(CAN_FD_CONTROL_REGISTER, TDCOFF,  8,  5)
    FIELD(CAN_FD_CONTROL_REGISTER, TDCVAL,  0,  6)

static uint64_t flexcan_reg_prewrite(RegisterInfo *reg, uint64_t val64)
{
    uint32_t val = val64;

    return val;
}

static void flexcan_mcr_postwrite(RegisterInfo *reg, uint64_t val64)
{
    FslFlexCanState *s = FSL_FLEXCAN(reg->opaque);

    if (!ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS)) {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, LPMACK, 0);
    } else {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, LPMACK, 1);
    }

    if (ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, SOFTRST)) {
        // These registers are affected by soft reset:
        /* From Table 377, S32G3 Reference Manual Rev 3.1
           MCR
           TIMER
           ECR
           ESR1
           IMASK2
           IMASK1
           IFLAG2
           IFLAG1
           ESR2
           CRCR
           IMASK4
           IMASK3
           IFLAG4
           IFLAG3
           MECR
           ERRIAR
           ERRIDPR
           ERRIPPR
           RERRAR
           RERRDR
           RERRSYNR
           ERRSR
           FDCRC
           ERFCR
           ERFIER
           ERFSR
        */
        {
            // MDIS bit in MCR is not affected by soft reset
            uint8_t mdis = ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS);
            register_reset(&s->reg_info[R_MODULE_CONFIGURATION_REGISTER]);
            ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS, mdis);
        }
        register_reset(&s->reg_info[R_INTERRUPT_MASKS_2_REGISTER]);
        register_reset(&s->reg_info[R_INTERRUPT_MASKS_1_REGISTER]);
        register_reset(&s->reg_info[R_MEMORY_ERROR_CONTROL_REGISTER]);
        register_reset(&s->reg_info[R_CAN_FD_CONTROL_REGISTER]);
        // clear soft reset bit once registers have been reset
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, SOFTRST, 0);
    }

    if (ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, HALT) &&
        ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZ)) {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK, 1);
    } else {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK, 0);
    }
}

static const RegisterAccessInfo flexcan_regs_info[] = {
    {   .name = "MODULE_CONFIGURATION_REGISTER",
        .addr = A_MODULE_CONFIGURATION_REGISTER,
        .reset = 0xD890000F,
        .pre_write = flexcan_reg_prewrite,
        .post_write = flexcan_mcr_postwrite,
    },{ .name = "CONTROL_1_REGISTER",
        .addr = A_CONTROL_1_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "RX_MAILBOXES_GLOBAL_MASK_REGISTER",
        .addr = A_RX_MAILBOXES_GLOBAL_MASK_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "RX_14_MASK_REGISTER",
        .addr = A_RX_14_MASK_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "RX_15_MASK_REGISTER",
        .addr = A_RX_15_MASK_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "INTERRUPT_MASKS_2_REGISTER",
        .addr = A_INTERRUPT_MASKS_2_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "INTERRUPT_MASKS_1_REGISTER",
        .addr = A_INTERRUPT_MASKS_1_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "CONTROL_2_REGISTER",
        .addr = A_CONTROL_2_REGISTER,
        .reset = 0x00100000,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "MEMORY_ERROR_CONTROL_REGISTER",
        .addr = A_MEMORY_ERROR_CONTROL_REGISTER,
        .reset = 0x800C0080,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "CAN_FD_CONTROL_REGISTER",
        .addr = A_CAN_FD_CONTROL_REGISTER,
        .reset = 0x80000100,
        .pre_write = flexcan_reg_prewrite,
    }
};

static void flexcan_mb_write(void *opaque, hwaddr offset,
                             uint64_t value, unsigned size)
{
}

static uint64_t flexcan_mb_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    return 0;
}

static const MemoryRegionOps flexcan_mb_ops = {
    .read = flexcan_mb_read,
    .write = flexcan_mb_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps flexcan_reg_ops = {
    .read = register_read_memory,
    .write = register_write_memory,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void flexcan_realize(DeviceState *dev, Error **errp)
{
    FslFlexCanState *s = FSL_FLEXCAN(dev);
    int i;

    for (i = 0; i < ARRAY_SIZE(flexcan_regs_info); i++) {
        int index = flexcan_regs_info[i].addr / 4;
        RegisterInfo *r = &s->reg_info[index];

        object_initialize(r, sizeof(*r), TYPE_REGISTER);

        *r = (RegisterInfo) {
            .data = &s->regs[index],
            .data_size = sizeof(uint32_t),
            .access = &flexcan_regs_info[i],
            .opaque = OBJECT(s),
        };
    }

    // add mem region for registers to the top level memory region
    memory_region_init_io(&s->reg_mem, OBJECT(s), &flexcan_reg_ops,
                          s, "flexcan.reg", FLEXCAN_R_SZ);
    memory_region_add_subregion(&s->flexcan, 0x0, &s->reg_mem);

    // add mem region for message buffers to top level memory region
    memory_region_init_io(&s->mb_mem, OBJECT(s), &flexcan_mb_ops,
                          s, "flexcan.mb", FLEXCAN_MB_SZ);
    memory_region_add_subregion(&s->flexcan, 0x80, &s->mb_mem);

    // init the top level memory region for mmio
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->flexcan);

    // 4 irqs per instance
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_state);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_berr);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_mb_0_7);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_mb_8_127);
    // todo: connect s->canbus
}

static void flexcan_reset(DeviceState *dev)
{
    FslFlexCanState *s = FSL_FLEXCAN(dev);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->reg_info); ++i) {
        register_reset(&s->reg_info[i]);
    }
}

static void flexcan_init(Object *obj)
{
    FslFlexCanState *s = FSL_FLEXCAN(obj);
    /*
     * init parent memory region of size large enough for entire mmio space
     */
    memory_region_init(&s->flexcan, obj, TYPE_FSL_FLEXCAN,
                       FLEXCAN_MMIO_SZ);
}

static const VMStateDescription vmstate_flexcan = {
    .name = TYPE_FSL_FLEXCAN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, FslFlexCanState, FLEXCAN_R_MAX),
        VMSTATE_END_OF_LIST(),
    }
};

static Property flexcan_core_properties[] = {
    // todo: other properties?
    DEFINE_PROP_LINK("canbus", FslFlexCanState, canbus,
                     TYPE_CAN_BUS, CanBusState *),
    DEFINE_PROP_END_OF_LIST(),
};

static void flexcan_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->reset = flexcan_reset;
    dc->realize = flexcan_realize;
    device_class_set_props(dc, flexcan_core_properties);
    dc->vmsd = &vmstate_flexcan;
}

static const TypeInfo flexcan_info = {
    .name           = TYPE_FSL_FLEXCAN,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(FslFlexCanState),
    .class_init     = flexcan_class_init,
    .instance_init  = flexcan_init,
};

static void flexcan_register_types(void)
{
    type_register_static(&flexcan_info);
}

type_init(flexcan_register_types)
