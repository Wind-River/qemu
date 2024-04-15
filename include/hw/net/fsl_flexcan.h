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

// this is the highest offset register in the mmio space
#define OFFSET_ERFFEL127    0x31FC

// 4 byte wide registers
#define FLEXCAN_R_MAX       (OFFSET_ERFFEL127 / 4) + 1

typedef struct FslFlexCanState {
    MemoryRegion        iomem;
    qemu_irq            canfd_irq;
    CanBusClientState   bus_client;
    CanBusState         *flexcanbus;
    uint32_t            regs[FLEXCAN_R_MAX];
} FslFlexCanState;

#endif // HW_FLEXCAN_FSL_H
