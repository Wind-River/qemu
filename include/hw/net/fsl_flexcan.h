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
#define FLEXCAN_R_SZ        FLEXCAN_R_MAX * 4

// 128 16 byte message buffers
#define FLEXCAN_MB_SZ       128 * 16

// message buffer start offset
#define FLEXCAN_MB_BASE     FLEXCAN_R_SZ

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
    uint8_t             mb[FLEXCAN_MB_SZ];
} FslFlexCanState;

#endif // HW_FLEXCAN_FSL_H
