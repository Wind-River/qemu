/*
 * xlnx-versal-lpd-iou-slcr.h
 *
 * QEMU model of Xilinx LPD System Level Control Registers for IOU
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

#ifndef XLNX_VERSAL_LPD_IOU_SLCR_H
#define XLNX_VERSAL_LPD_IOU_SLCR_H

#include "qom/object.h"

#define TYPE_XLNX_LPD_IOU_SLCR "xlnx.versal-lpd-iou-slcr"

OBJECT_DECLARE_SIMPLE_TYPE(LPD_IOU_SLCR, XLNX_LPD_IOU_SLCR)

#define XILINX_LPD_IOU_SLCR(obj) \
     OBJECT_CHECK(LPD_IOU_SLCR, (obj), TYPE_XLNX_LPD_IOU_SLCR)

#define LPD_IOU_SLCR_R_MAX 459

typedef struct LPD_IOU_SLCR {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_parity_imr;
    qemu_irq irq_imr;

    uint32_t regs[LPD_IOU_SLCR_R_MAX];
    RegisterInfo regs_info[LPD_IOU_SLCR_R_MAX];
} LPD_IOU_SLCR;

#endif /* XLNX_VERSAL_LPD_IOU_SLCR_H */
