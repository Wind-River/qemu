/*
 * fsl_flexcan.c, Freescale FlexCAN Controller device model
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
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "net/can_emu.h"
#include "net/can_host.h"
#include "hw/net/fsl_flexcan.h"

REG32(MODULE_CONFIGURATION_REGISTER, 0x0)
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
REG32(ERROR_AND_STATUS_1_REGISTER, 0x20)
    FIELD(ERROR_AND_STATUS_1_REGISTER, ERRINT,      1,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BOFFINT,     2,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, RX,          3,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, FLTCONF,     4,  2)
    FIELD(ERROR_AND_STATUS_1_REGISTER, TX,          6,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, IDLE,        7,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, RXWRN,       8,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, TXWRN,       9,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, STFERR,      10,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, FRMERR,      11,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, CRCERR,      12,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, ACKERR,      13,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BIT0ERR,     14,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BIT1ERR,     15,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, RWRNINT,     16,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, TWRNINT,     17,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, SYNCH,       18,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BOFFDONEINT, 19,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, ERRINT_FAST, 20,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, ERROVR,      21,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, STFERR_FAST, 26,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, FRMERR_FAST, 27,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, CRCERR_FAST, 28,  1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BIT0ERR_FAST, 30, 1)
    FIELD(ERROR_AND_STATUS_1_REGISTER, BIT1ERR_FAST, 31, 1)
REG32(INTERRUPT_MASKS_2_REGISTER, 0x24)
    FIELD(INTERRUPT_MASKS_2_REGISTER, BUF63TO32M,   0, 32)
REG32(INTERRUPT_MASKS_1_REGISTER, 0x28)
    FIELD(INTERRUPT_MASKS_1_REGISTER, BUF31TO0M,    0, 32)
REG32(INTERRUPT_FLAGS_1_REGISTER, 0x30)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF0I,        0,  1)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF4TO1I,     1,  4)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF5I,        5,  1)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF6I,        6,  1)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF7I,        7,  1)
    FIELD(INTERRUPT_FLAGS_1_REGISTER, BUF31TO8I,    8,  24)
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

/* MECR */
#define MECR_OFFSET         0xAE0
#define MECR_NCEFAFRZ       (1 << 7)
#define MECR_ECCDIS         (1 << 8)
#define MECR_RERRDIS        (1 << 9)
#define MECR_EXTERRIE       (1 << 13)
#define MECR_FAERRIE        (1 << 14)
#define MECR_HAERRIE        (1 << 15)
#define MECR_ECRWRDIS       (1 << 31)

/* FDCTRL */
#define FDCTRL_OFFSET       0xC00
#define FDCTRL_TDCVAL       (0x3F << 0)
#define FDCTRL_TDCOFF       (0x1F << 8)
#define FDCTRL_TDCFAIL      (0x1 << 14)
#define FDCTRL_TDCEN        (0x1 << 15)
#define FDCTRL_MBDSR0       (0x3 << 16)
#define FDCTRL_MBDSR1       (0x3 << 19)
#define FDCTRL_MBDSR2       (0x3 << 22)
#define FDCTRL_MBDSR3       (0x3 << 25)
#define FDCTRL_FDRATE       (0x1 << 31)

static void flexcan_update_irq(FslFlexCanState *s)
{
    // INTERRUPT_MASKS_1 and INTERRUPT_FLAGS_1 use 1 bit per message buffer
    if (s->regs[R_INTERRUPT_MASKS_1_REGISTER] & s->regs[R_INTERRUPT_FLAGS_1_REGISTER]) {
        qemu_set_irq(s->irq_state, 1);
    } else {
        qemu_set_irq(s->irq_state, 0);
    }
}

static uint64_t flexcan_reg_prewrite(RegisterInfo *reg, uint64_t val64)
{
    uint32_t val = val64;

    return val;
}

static void flexcan_mcr_postwrite(RegisterInfo *reg, uint64_t val64)
{
    FslFlexCanState *s = FSL_FLEXCAN(reg->opaque);

    if (ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS)) {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, LPMACK, 1);
        ARRAY_FIELD_DP32(s->regs, ERROR_AND_STATUS_1_REGISTER, SYNCH, 1);
    } else {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, LPMACK, 0);
        ARRAY_FIELD_DP32(s->regs, ERROR_AND_STATUS_1_REGISTER, SYNCH, 0);
    }

    if (ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, HALT) &&
        ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZ)) {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK, 1);
    } else {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK, 0);
    }

    if (ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS) ||
        ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZ)) {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, NOTRDY, 1);
    } else {
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, NOTRDY, 0);
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
            // MDIS, NOTRDY, SOFTRST, FRZACK, LPMACK bit in MCR is not affected by soft reset
            uint32_t save = 0;
            save |= ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, MDIS);
            save |= ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, NOTRDY);
            save |= ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, SOFTRST);
            save |= ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK);
            save |= ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, LPMACK);
            register_reset(&s->reg_info[R_MODULE_CONFIGURATION_REGISTER]);
            s->regs[R_MODULE_CONFIGURATION_REGISTER] |= save;
        }
        register_reset(&s->reg_info[R_ERROR_AND_STATUS_1_REGISTER]);
        register_reset(&s->reg_info[R_INTERRUPT_MASKS_2_REGISTER]);
        register_reset(&s->reg_info[R_INTERRUPT_MASKS_1_REGISTER]);
        // clear soft reset bit once registers have been reset
        ARRAY_FIELD_DP32(s->regs, MODULE_CONFIGURATION_REGISTER, SOFTRST, 0);
    }
}

static void flexcan_imask_postwrite(RegisterInfo *reg, uint64_t val64)
{
    FslFlexCanState *s = FSL_FLEXCAN(reg->opaque);

    flexcan_update_irq(s);
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
    },{ .name = "ERROR_AND_STATUS_1_REGISTER",
        .addr = A_ERROR_AND_STATUS_1_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "INTERRUPT_MASKS_2_REGISTER",
        .addr = A_INTERRUPT_MASKS_2_REGISTER,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "INTERRUPT_MASKS_1_REGISTER",
        .addr = A_INTERRUPT_MASKS_1_REGISTER,
        .pre_write = flexcan_reg_prewrite,
        .post_write = flexcan_imask_postwrite,
    },{ .name = "CONTROL_2_REGISTER",
        .addr = A_CONTROL_2_REGISTER,
        .reset = 0x00100000,
        .pre_write = flexcan_reg_prewrite,
    },{ .name = "INTERRUPT_FLAGS_1_REGISTER",
        .addr = A_INTERRUPT_FLAGS_1_REGISTER,
        .w1c = 0xFFFFFFFF,
        .pre_write = flexcan_reg_prewrite,
    }
};

static ssize_t flexcan_receive(CanBusClientState *client,
                               const qemu_can_frame *buf,
                               size_t buf_size)
{
    // currently only support legacy Rx Fifo
    // Legacy Rx FIFO occupies 0x80 to 0xDC, which is normally occupied by MB0-MB5
    FslFlexCanState *s = container_of(client, FslFlexCanState, bus_client);
    const qemu_can_frame *frame = buf;
    int i;

    if (buf_size <= 0) {
        return 0;
    }

    // write length of data payload in bytes
    s->mb[0] = 0;
    s->mb[0] = FLEXCAN_MB_CTRL_DLC_W(s->mb[0], frame->can_dlc);

    // write CAN ID and handle extended ID flag
    s->mb[1] = 0;
    if (frame->can_id & QEMU_CAN_EFF_FLAG) {
        s->mb[0] = FLEXCAN_MB_CTRL_IDE_W(s->mb[0], 1);
        s->mb[1] = FLEXCAN_MB_ID_EXT_W(s->mb[1], frame->can_id);
    } else {
        s->mb[1] = FLEXCAN_MB_ID_STD_W(s->mb[1], frame->can_id);
    }

    // zero payload buffer
    memset(&s->mb[2], 0, frame->can_dlc); // first 2 double words are header

    // set data payload registers according to received data
    for (i = 0; i < frame->can_dlc; i++) {
        s->mb[2 + (i / 4)] |= frame->data[i] << ((i % 4) * 8);
    }

    // set interrupt flag corresponding to legacy Rx FIFO
    ARRAY_FIELD_DP32(s->regs, INTERRUPT_FLAGS_1_REGISTER, BUF5I, 1);

    // raise interrupt
    flexcan_update_irq(s);

    return buf_size;
}

static bool flexcan_can_receive(CanBusClientState *client)
{
    FslFlexCanState *s = container_of(client, FslFlexCanState, bus_client);

    // currently only support Legacy Rx FIFO
    if (!ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, RFEN)) {
        return false;
    }

    // if legacy Rx FIFO mode is enabled, the BUF5I flag indicates that there
    // is a valid frame in the legacy Rx FIFO
    // the flag is cleared by software once it consumes the buffer contents
    return !ARRAY_FIELD_EX32(s->regs, INTERRUPT_FLAGS_1_REGISTER, BUF5I);
}

static CanBusClientInfo flexcan_bus_client_info = {
    .can_receive = flexcan_can_receive,
    .receive = flexcan_receive,
};

static void flexcan_connect_canbus(FslFlexCanState *s, Error **errp)
{
    if (s->canbus) {
        s->bus_client.info = &flexcan_bus_client_info;
        if (can_bus_insert_client(s->canbus, &s->bus_client) < 0) {
            error_setg(errp, "fsl_flexcan can_bus_insert_client failed");
        }
    }
}

static void flexcan_mb_write(void *opaque, hwaddr offset,
                             uint64_t value, unsigned size)
{
    FslFlexCanState *s = opaque;
    uint32_t mb_idx = offset >> 2;
    qemu_can_frame frame;
    int i;

    // write the new value to s->mb
    s->mb[mb_idx] = (uint32_t)value;

    // TODO: Currently assuming legacy mode with 8 byte payload / 16 byte messages
    // MB control lives at offset 0 from the start of each message buffer
    if (!(offset % FLEXCAN_MB_SIZE)) {
        // writing to MB control
        if (!ARRAY_FIELD_EX32(s->regs, MODULE_CONFIGURATION_REGISTER, FRZACK) &&
            FLEXCAN_MB_CTRL_CODE_R(s->mb[mb_idx]) == FLEXCAN_MB_TX_CODE_DATA) {
            // set can ID and extended ID flag
            frame.can_id = FLEXCAN_MB_CTRL_IDE_R(s->mb[mb_idx]) ?
                            FLEXCAN_MB_ID_EXT_R(s->mb[mb_idx + 1]) | QEMU_CAN_EFF_FLAG :
                            FLEXCAN_MB_ID_STD_R(s->mb[mb_idx + 1]);
            frame.can_dlc = FLEXCAN_MB_CTRL_DLC_R(s->mb[mb_idx]);
            // extract the data bytes
            for (i = 0; i < FLEXCAN_MB_DATA_LEN_LEGACY; i++) {
                frame.data[i] = (s->mb[mb_idx + (i/4) + 2] >> ((i % 4) * 8)) & 0xFF;
            }
            /*
             * TODO: Currently assuming all writes to messages buffer are TX.
             *
             * CTRL2[RFFN] indicates number of legacy RX fifos
             * switch CTRL2[RFFN]:
             *      case 0:
             *          RX MB[0..7]
             *          TX MB[8..MCR[MAXMB]]
             *      case 1:
             *          RX MB[0..9]
             *          TX MB[10..MCR[MAXMB]]
             *      case 2:
             *          RX MB[0..11]
             *          TX MB[12..MCR[MAXMB]]
             *      case 3:
             *          RX MB[0..13]
             *          TX MB[14..MCR[MAXMB]]
             *      case 4:
             *          RX MB[0..15]
             *          TX MB[16..MCR[MAXMB]]
             *      ...
             *
             *  In legacy (non-FD) mode, the maximum value of MCR[MAXMB] is 127
             *
             *  It is the software's responsibility to program MCR[MAXMB]
             *  to a value that is compatible with the device configuration.
             *
             *  VxWorks configures MCR[MAXMB] to 8, so that MB8 is the only
             *  message buffer used for TX.
             *  
             *  VxWorks does not use IFLAGS[2..4] registers since it does not
             *  use MBs higher than 8.
             */
            can_bus_client_send(&s->bus_client, &frame, 1);
            ARRAY_FIELD_DP32(s->regs, INTERRUPT_FLAGS_1_REGISTER, BUF31TO8I, 1);
        }
    }
    flexcan_update_irq(s);
}

static uint64_t flexcan_mb_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    FslFlexCanState *s = opaque;
    uint32_t mb_idx = offset >> 2;

    return s->mb[mb_idx];
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
    RegisterInfoArray *reg_array;

    reg_array = register_init_block32(dev, flexcan_regs_info,
                                      ARRAY_SIZE(flexcan_regs_info),
                                      s->reg_info,
                                      s->regs,
                                      &flexcan_reg_ops,
                                      NULL,
                                      FLEXCAN_BASE_R_LEN);

    // add mem region for registers to the top level memory region
    memory_region_add_subregion(&s->flexcan, 0x0, &reg_array->mem);

    // add mem region for message buffers to top level memory region
    memory_region_init_io(&s->mb_mem, OBJECT(s), &flexcan_mb_ops,
                          s, "flexcan.mb", FLEXCAN_MB_LEN);
    memory_region_add_subregion(&s->flexcan, FLEXCAN_MB_BASE, &s->mb_mem);

    // init the top level memory region for mmio
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->flexcan);

    // 4 irqs per instance
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_state);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_berr);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_mb_0_7);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq_mb_8_127);

    flexcan_connect_canbus(s, errp);
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
        VMSTATE_UINT32_ARRAY(regs, FslFlexCanState, FLEXCAN_BASE_REGS),
        VMSTATE_UINT32_ARRAY(mb, FslFlexCanState, FLEXCAN_MB_REGS),
        VMSTATE_UINT32_ARRAY(fd_regs, FslFlexCanState, FLEXCAN_FD_REGS),
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
