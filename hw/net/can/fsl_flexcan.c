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
REG32(RX_MAILBOXES_GLOBAL_MASK_REGISTER, 0x10)
    FIELD(RX_MAILBOXES_GLOBAL_MASK_REGISTER, MG, 0, 32)
REG32(RX_14_MASK_REGISTER, 0x14)
    FIELD(RX_14_MASK_REGISTER, RX14M, 0, 32)
REG32(RX_15_MASK_REGISTER, 0x18)
    FIELD(RX_15_MASK_REGISTER, RX15M, 0, 32)
REG32(MEMORY_ERROR_CONTROL_REGISTER, 0xAE0)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, ECRWRDIS,  31, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, HAERRIE,   15, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, FAERRIE,   14, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, EXTERRIE,  13, 1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, RERRDIS,   9,  1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, ECCDIS,    8,  1)
    FIELD(MEMORY_ERROR_CONTROL_REGISTER, NCEFAFRZ,  7,  1)
REG32(INTERRUPT_MASKS_2_REGISTER, 0x24)
    FIELD(INTERRUPT_MASKS_2_REGISTER, BUF63TO32M,   0, 32)
REG32(INTERRUPT_MASKS_1_REGISTER, 0x28)
    FIELD(INTERRUPT_MASKS_1_REGISTER, BUF31TO0M,    0, 32)

static void flexcan_realize(DeviceState *dev, Error **errp)
{
    FslFlexCanState *s = FSL_FLEXCAN(dev);

    // todo: look into use of RegisterInfoArray subregion
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->canfd_irq);
    // todo: connect s->flexcanbus
}

static void flexcan_reset(DeviceState *dev)
{
    FslFlexCanState *s = FSL_FLEXCAN(dev);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->regs); ++i) {
        // todo: reset to correct values
        s->regs[i] = 0;
    }
}

static void flexcan_init(Object *obj)
{
    FslFlexCanState *s = FSL_FLEXCAN(obj);

    memory_region_init(&s->iomem, obj, TYPE_FSL_FLEXCAN,
                       FLEXCAN_R_MAX * 4);
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
    DEFINE_PROP_LINK("flexcanbus", FslFlexCanState, flexcanbus,
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
