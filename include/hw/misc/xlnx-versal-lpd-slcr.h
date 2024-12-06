/*
 * xlnx-versal-lpd-slcr.h
 *
 * QEMU model of Xilinx LPD System Level Control Registers
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
 */

#ifndef XLNX_VERSAL_LPD_SLCR_H
#define XLNX_VERSAL_LPD_SLCR_H

#include "hw/register.h"
#include "qom/object.h"

#define TYPE_XILINX_LPD_SLCR "xlnx.versal-lpd-slcr"

#define XILINX_LPD_SLCR(obj) \
     OBJECT_CHECK(LPD_SLCR, (obj), TYPE_XILINX_LPD_SLCR)

#define LPD_SLCR_R_MAX 83

typedef struct LPD_SLCR {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_imr;

    uint32_t regs[LPD_SLCR_R_MAX];
    RegisterInfo regs_info[LPD_SLCR_R_MAX];
} LPD_SLCR;

#endif /* XLNX_VERSAL_LPD_SLCR_H */
