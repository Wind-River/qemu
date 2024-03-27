/*
 * QEMU model of Freescale LINFlex UART
 *
 * Copyright (c) 2024 Wind River Systems, Inc.
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

#ifndef FSL_LINFLEXUART_H
#define FSL_LINFLEXUART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define TYPE_FSL_LINFLEX    "fsl.s32v234-linflexuart"
OBJECT_DECLARE_SIMPLE_TYPE(FslLinflexState, FSL_LINFLEX)

/* Register Mapping */
typedef enum {
    LINFLEX_LINCR1,
    LINFLEX_LINIER,
    LINFLEX_LINSR,
    LINFLEX_LINESR,
    LINFLEX_UARTCR,
    LINFLEX_UARTSR,
    LINFLEX_LINTCSR,
    LINFLEX_LINOCR,
    LINFLEX_LINTOCR,
    LINFLEX_LINFBRR,
    LINFLEX_LINIBRR,
    LINFLEX_LINCFR,
    LINFLEX_LINCR2,
    LINFLEX_BIDR,
    LINFLEX_BDRL,
    LINFLEX_BDRM,
    LINFLEX_GCR = 19, // gap here
    LINFLEX_UARTPTO,
    LINFLEX_UARTCTO,
    LINFLEX_DMATXE,
    LINFLEX_DMARXE,
    LINFLEX_REGS_MAX,
} LinflexRegs;

struct FslLinflexState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[LINFLEX_REGS_MAX];

    CharBackend chr;
    qemu_irq irq;
};

/* LINFlex Control Register */
#define LINCR1_INIT         (0x1 << 0)

/* LINFlex Status Register */
#define LINSR_LINS_MASK     (0xF << 15)
#define LINSR_LINS_INIT     (0x1 << 12)

/* LINFlex Interrupt Enable Register */
#define LINIER_DTIE         (0x1 << 1)
#define LINIER_DRIE         (0x1 << 2)

/* UART Control Register Fields */
#define UARTCR_UART         (0x1 << 0)
#define UARTCR_WL0          (0x1 << 1)
#define UARTCR_PCE          (0x1 << 2)
#define UARTCR_PC0          (0x1 << 3)
#define UARTCR_TXEN         (0x1 << 4)
#define UARTCR_RXEN         (0x1 << 5)
#define UARTCR_PC1          (0x1 << 6)
#define UARTCR_WL1          (0x1 << 7)
#define UARTCR_TFBM         (0x1 << 8)
#define UARTCR_RFBM         (0x1 << 9)
#define UARTCR_RDFL_RFC     (0x7 << 12)
#define UARTCR_TDFL_TFC     (0x7 << 15)
#define UARTCR_SBUR         (0x3 << 18)
#define UARTCR_DTU_PCETX    (0x1 << 19)
#define UARTCR_NEF          (0x7 << 22)
#define UARTCR_ROSE         (0x1 << 23)
#define UARTCR_OSR          (0xF << 27)
#define UARTCR_CSP          (0x7 << 30)
#define UARTCR_MIS          (0x1 << 31)

/* UART Status Register Fields */
#define UARTSR_NF           (0x1 << 0)
#define UARTSR_DTFTFF       (0x1 << 1)
#define UARTSR_DRFRFE       (0x1 << 2)
#define UARTSR_TO           (0x1 << 3)
#define UARTSR_RFNE         (0x1 << 4)
#define UARTSR_WUF          (0x1 << 5)
#define UARTSR_RDI          (0x1 << 6)
#define UARTSR_BOF          (0x1 << 7)
#define UARTSR_FEF          (0x1 << 8)
#define UARTSR_RMB          (0x1 << 9)
#define UARTSR_PE           (0xF << 13)
#define UARTSR_OCF          (0x1 << 14)
#define UARTSR_SZF          (0x1 << 15)

#endif // FSL_LINFLEXUART_H
