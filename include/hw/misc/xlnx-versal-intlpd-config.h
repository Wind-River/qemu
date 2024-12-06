/*
 * xlnx-versal-intlpd-config.h
 *
 * QEMU model of Xilinx LPD interrupt configuration and status registers
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

#ifndef XLNX_VERSAL_INTLPD_CONFIG_H
#define XLNX_VERSAL_INTLPD_CONFIG_H

#include "hw/misc/xlnx-serbs.h"
#include "qom/object.h"

#define TYPE_XILINX_INTLPD_CONFIG "xlnx-intlpd-config"

#define XILINX_INTLPD_CONFIG(obj) \
     OBJECT_CHECK(INTLPD_CONFIG, (obj), TYPE_XILINX_INTLPD_CONFIG)

#define AFIFS_SERBS_ID 1

#define INTLPD_CONFIG_R_MAX 44

typedef struct INTLPD_CONFIG {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq_ir_mission1;
    qemu_irq irq_ir_mission2;
    qemu_irq irq_ir;
    qemu_irq irq_ir_perr;

    xlnx_serbs_if *AFIFSSerbs;
    int tov;
    uint32_t regs[INTLPD_CONFIG_R_MAX];
    RegisterInfo regs_info[INTLPD_CONFIG_R_MAX];
} INTLPD_CONFIG;

#endif /* XLNX_VERSAL_INTLPD_CONFIG_H */
