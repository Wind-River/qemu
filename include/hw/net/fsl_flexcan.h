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
#define FLEXCAN_R_MAX       32

// 32 * 4 bytes worth of register space starting at module base addr
#define FLEXCAN_R_LEN       FLEXCAN_R_MAX * 4

// 128 16 byte message buffers
#define FLEXCAN_NUM_MB      128
#define FLEXCAN_MB_SIZE     16

// message buffer start offset
#define FLEXCAN_MB_BASE     FLEXCAN_R_LEN

// message buffer length
#define FLEXCAN_MB_LEN      FLEXCAN_NUM_MB * FLEXCAN_MB_SIZE

/*
 * The message buffers on the FlexCAN are implemented using memory mapped
 * embedded RAM and are not actually registers. Accesses and fields are
 * 4 byte aligned, so it can be helpful to think of them as registers.
 */
#define FLEXCAN_MB_REGS     FLEXCAN_MB_LEN / 4

// FlexCAN Message Buffer fields
#define FLEXCAN_MB_CTRL_DLC(x)      ((x >> 16) & 0xF)
#define FLEXCAN_MB_CTRL_IDE(x)      ((x >> 21) & 0x1)
#define FLEXCAN_MB_CTRL_CODE(x)     ((x >> 24) & 0xF)
#define FLEXCAN_MB_CTRL_EDL(x)      ((x >> 31) & 0x1)
#define FLEXCAN_MB_ID_STANDARD(x)   ((x >> 18) & 0x7FF)
#define FLEXCAN_MB_ID_EXTENDED(x)   (x & 0x1FFFFFFF)

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
    uint32_t            regs[FLEXCAN_R_MAX];
    RegisterInfo        reg_info[FLEXCAN_R_MAX];
    uint32_t            mb[FLEXCAN_MB_REGS];
} FslFlexCanState;

#endif // HW_FLEXCAN_FSL_H
