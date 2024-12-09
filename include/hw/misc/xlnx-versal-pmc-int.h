/*
 * xlnx-versal-pmc-int.h
 *
 * QEMU model of the PMC_INT_REGS PMC Interconnect isolation, reset and status registers
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

#ifndef XLNX_VERSAL_PMC_INT_H
#define XLNX_VERSAL_PMC_INT_H

#include "qom/object.h"

#define TYPE_XILINX_PMC_INT_REGS "xlnx.versal-pmc-int"
OBJECT_DECLARE_SIMPLE_TYPE(PMC_INT_REGS, XILINX_PMC_INT_REGS)

#define XILINX_PMC_INT_REGS(obj) \
     OBJECT_CHECK(PMC_INT_REGS, (obj), TYPE_XILINX_PMC_INT_REGS)

#define PMC_INT_REGS_R_MAX 44

typedef struct PMC_INT_REGS {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_int_pmc_timeout_imr;
    qemu_irq irq_int_pmc_parity1_imr;
    qemu_irq irq_ir;
    qemu_irq irq_int_pmc_parity0_imr;
    qemu_irq irq_int_pmc_mission_imr;

    uint32_t regs[PMC_INT_REGS_R_MAX];
    RegisterInfo regs_info[PMC_INT_REGS_R_MAX];
} PMC_INT_REGS;


#endif /* XLNX_VERSAL_PMC_INT_H */
