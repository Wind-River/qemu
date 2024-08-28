/*
 * fsl_flexcan.h, Freescale FlexCAN Controller device model
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


#ifndef HW_FLEXCAN_FSL_H
#define HW_FLEXCAN_FSL_H

#include "hw/register.h"
#include "net/can_emu.h"
#include "hw/qdev-clock.h"

#define TYPE_FSL_FLEXCAN "fsl.flexcan"

OBJECT_DECLARE_SIMPLE_TYPE(FslFlexCanState, FSL_FLEXCAN)

/*
 * From RMS32G3 Rev 3.1 Section 51.6.1
 * The address space occupied by FlexCAN has 128 bytes for registers
 * starting at the module base address, followed by embedded RAM
 * starting at address offset 0080h.
 */

// 32 4 byte wide registers
#define FLEXCAN_BASE_REGS       32

// 32 * 4 bytes worth of register space starting at module base addr
#define FLEXCAN_BASE_R_LEN      FLEXCAN_BASE_REGS * 4

// 128 16 byte message buffers
#define FLEXCAN_NUM_MB          128
#define FLEXCAN_MB_SIZE         16

// message buffer start offset
#define FLEXCAN_MB_BASE         FLEXCAN_BASE_R_LEN

// message buffer length
#define FLEXCAN_MB_LEN          FLEXCAN_NUM_MB * FLEXCAN_MB_SIZE

// offset of CAN FD registers
#define FLEXCAN_FD_REG_OFF      0xC00

// number of CAN FD registers
#define FLEXCAN_FD_REGS         6

/*
 * The message buffers on the FlexCAN are implemented using memory mapped
 * embedded RAM and are not actually registers. Accesses and fields are
 * 4 byte aligned, so it can be helpful to think of them as registers.
 */
#define FLEXCAN_MB_REGS     FLEXCAN_MB_LEN / 4

// FlexCAN Message Buffer fields
#define FLEXCAN_MB_CTRL_DLC_R(x)        ((x >> 16) & 0xF)
#define FLEXCAN_MB_CTRL_DLC_W(d,v)      (d | ((v & 0xF) << 16))
#define FLEXCAN_MB_CTRL_IDE_R(x)        ((x >> 21) & 0x1)
#define FLEXCAN_MB_CTRL_IDE_W(d,v)      (d | ((v & 0x1) << 21))
#define FLEXCAN_MB_CTRL_CODE_R(x)       ((x >> 24) & 0xF)
#define FLEXCAN_MB_CTRL_EDL_R(x)        ((x >> 31) & 0x1)
#define FLEXCAN_MB_ID_STD_R(x)          ((x >> 18) & QEMU_CAN_SFF_MASK)
#define FLEXCAN_MB_ID_STD_W(d,v)        (d | ((v & QEMU_CAN_SFF_MASK) << 18))
#define FLEXCAN_MB_ID_EXT_R(x)          (x & QEMU_CAN_EFF_MASK)
#define FLEXCAN_MB_ID_EXT_W(d,v)        (d | (v & QEMU_CAN_EFF_MASK))

// FlexCAN Message Buffer Code Rx
#define FLEXCAN_MB_RX_CODE_INACTIVE     0x0
#define FLEXCAN_MB_RX_CODE_EMPTY        0x4
#define FLEXCAN_MB_RX_CODE_FULL         0x2

// FlexCAN Message Buffer Code Tx
#define FLEXCAN_MB_TX_CODE_INACTIVE     0x8
#define FLEXCAN_MB_TX_CODE_ABORT        0x9
#define FLEXCAN_MB_TX_CODE_DATA         0xC

// FlexCAN Message Buffer Payload
// message buffer size configurable in CAN FD mode via FDCTRL[MBDSR]
#define FLEXCAN_MB_DATA_LEN_LEGACY      0x8

// this is the size advertised in the device tree
#define FLEXCAN_MMIO_SZ     0xA000

typedef struct FslFlexCanState {
    SysBusDevice        parent_obj;
    MemoryRegion        flexcan;
    MemoryRegion        reg_mem;
    MemoryRegion        mb_mem;
    qemu_irq            irq_state;
    qemu_irq            irq_berr;
    qemu_irq            irq_mb_0_7;
    qemu_irq            irq_mb_8_127;
    CanBusClientState   bus_client;
    CanBusState         *canbus;
    uint32_t            regs[FLEXCAN_BASE_REGS];
    RegisterInfo        reg_info[FLEXCAN_BASE_REGS];
    uint32_t            mb[FLEXCAN_MB_REGS];
    uint32_t            fd_regs[FLEXCAN_FD_REGS];
    RegisterInfo        fd_reg_info[FLEXCAN_FD_REGS];
} FslFlexCanState;

#endif // HW_FLEXCAN_FSL_H
