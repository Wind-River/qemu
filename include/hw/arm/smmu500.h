/*
 * smmu500.h, ARM MMU-500 SMMU model
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
 *
 * Copyright (c) 2014 Xilinx Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "qom/object.h"

#define TYPE_XILINX_SMMU500 "arm.mmu-500"
#define TYPE_XILINX_SMMU500_IOMMU_MEMORY_REGION "arm.mmu-500-iommu-memory-region"

OBJECT_DECLARE_SIMPLE_TYPE(SMMU500State, XILINX_SMMU500)

/* This should be configurable per instance.  */
#define PAGESIZE 4096

#define MAX_CB 128

#define R_MAX (2 * MAX_CB * PAGESIZE)

/* Maximum number of TBUs supported by this model.  */
#define MAX_TBU 16

/* Forward declaration */
struct SMMU500State;

typedef struct TBU {
    SMMU500State *smmu;
    IOMMUMemoryRegion iommu;
    AddressSpace *as;
} TBU;

typedef struct SMMU500State {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    MemoryRegion *dma_mr;
    AddressSpace dma_as;

    TBU tbu[MAX_TBU];
    uint8_t num_tbu;

    struct {
        qemu_irq global;
        qemu_irq context[MAX_CB];
    } irq;

    struct {
        uint32_t pamax;
        uint16_t num_smr;
        uint16_t num_cb;
        uint16_t num_pages;
        bool ato;
        uint8_t version;
    } cfg;

    RegisterAccessInfo *rai_smr;
    RegisterAccessInfo *rai_cb;
    uint32_t regs[R_MAX];
    RegisterInfo regs_info[R_MAX];
} SMMU500State;
