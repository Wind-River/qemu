/*
 * i.MX 8M Plus SoC Definitions
 *
 * Copyright (c) 2024, Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_H
#define FSL_IMX8MP_H

#include "cpu.h"
#include "hw/char/imx_serial.h"
#include "hw/gpio/imx_gpio.h"
#include "hw/i2c/imx_i2c.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/misc/imx7_snvs.h"
#include "hw/misc/imx8mp_analog.h"
#include "hw/misc/imx8mp_ccm.h"
#include "hw/net/imx_fec.h"
#include "hw/or-irq.h"
#include "hw/pci-host/designware.h"
#include "hw/pci-host/fsl_imx8m_phy.h"
#include "hw/sd/sdhci.h"
#include "hw/ssi/imx_spi.h"
#include "hw/timer/imx_gpt.h"
#include "hw/usb/hcd-dwc3.h"
#include "hw/watchdog/wdt_imx2.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "qemu/units.h"

#define TYPE_FSL_IMX8MP "fsl-imx8mp"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpState, FSL_IMX8MP)

#define FSL_IMX8MP_RAM_START        0x40000000
#define FSL_IMX8MP_RAM_SIZE_MAX     (8 * GiB)

enum FslImx8mpConfiguration {
    FSL_IMX8MP_NUM_CPUS         = 4,
    FSL_IMX8MP_NUM_ECSPIS       = 3,
    FSL_IMX8MP_NUM_GPIOS        = 5,
    FSL_IMX8MP_NUM_GPTS         = 6,
    FSL_IMX8MP_NUM_I2CS         = 6,
    FSL_IMX8MP_NUM_IRQS         = 160,
    FSL_IMX8MP_NUM_UARTS        = 4,
    FSL_IMX8MP_NUM_USBS         = 2,
    FSL_IMX8MP_NUM_USDHCS       = 3,
    FSL_IMX8MP_NUM_WDTS         = 3,
};

struct FslImx8mpState {
    SysBusDevice   parent_obj;

    ARMCPU             cpu[FSL_IMX8MP_NUM_CPUS];
    GICv3State         gic;
    IMXGPTState        gpt[FSL_IMX8MP_NUM_GPTS];
    IMXGPIOState       gpio[FSL_IMX8MP_NUM_GPIOS];
    IMX8MPCCMState     ccm;
    IMX8MPAnalogState  analog;
    IMX7SNVSState      snvs;
    IMXSPIState        spi[FSL_IMX8MP_NUM_ECSPIS];
    IMXI2CState        i2c[FSL_IMX8MP_NUM_I2CS];
    IMXSerialState     uart[FSL_IMX8MP_NUM_UARTS];
    IMXFECState        enet;
    SDHCIState         usdhc[FSL_IMX8MP_NUM_USDHCS];
    IMX2WdtState       wdt[FSL_IMX8MP_NUM_WDTS];
    USBDWC3            usb[FSL_IMX8MP_NUM_USBS];
    DesignwarePCIEHost pcie;
    FslImx8mPciePhyState   pcie_phy;
    OrIRQState         gpt5_gpt6_irq;
    MemoryRegion       ocram;

    uint32_t           phy_num;
    bool               phy_connected;
};

enum FslImx8mpMemoryRegions {
    FSL_IMX8MP_A53_DAP,
    FSL_IMX8MP_AIPS1_CONFIGURATION,
    FSL_IMX8MP_AIPS2_CONFIGURATION,
    FSL_IMX8MP_AIPS3_CONFIGURATION,
    FSL_IMX8MP_AIPS4_CONFIGURATION,
    FSL_IMX8MP_AIPS5_CONFIGURATION,
    FSL_IMX8MP_ANA_OSC,
    FSL_IMX8MP_ANA_PLL,
    FSL_IMX8MP_ANA_TSENSOR,
    FSL_IMX8MP_APBH_DMA,
    FSL_IMX8MP_ASRC,
    FSL_IMX8MP_AUDIO_BLK_CTRL,
    FSL_IMX8MP_AUDIO_DSP,
    FSL_IMX8MP_AUDIO_XCVR_RX,
    FSL_IMX8MP_AUD_IRQ_STEER,
    FSL_IMX8MP_BOOT_ROM,
    FSL_IMX8MP_BOOT_ROM_PROTECTED,
    FSL_IMX8MP_CAAM,
    FSL_IMX8MP_CAAM_MEM,
    FSL_IMX8MP_CCM,
    FSL_IMX8MP_CSU,
    FSL_IMX8MP_DDR_BLK_CTRL,
    FSL_IMX8MP_DDR_CTL,
    FSL_IMX8MP_DDR_PERF_MON,
    FSL_IMX8MP_DDR_PHY,
    FSL_IMX8MP_DDR_PHY_BROADCAST,
    FSL_IMX8MP_ECSPI1,
    FSL_IMX8MP_ECSPI2,
    FSL_IMX8MP_ECSPI3,
    FSL_IMX8MP_EDMA_CHANNELS,
    FSL_IMX8MP_EDMA_MANAGEMENT_PAGE,
    FSL_IMX8MP_ENET1,
    FSL_IMX8MP_ENET2_TSN,
    FSL_IMX8MP_FLEXCAN1,
    FSL_IMX8MP_FLEXCAN2,
    FSL_IMX8MP_GIC_DIST,
    FSL_IMX8MP_GIC_REDIST,
    FSL_IMX8MP_GPC,
    FSL_IMX8MP_GPIO1,
    FSL_IMX8MP_GPIO2,
    FSL_IMX8MP_GPIO3,
    FSL_IMX8MP_GPIO4,
    FSL_IMX8MP_GPIO5,
    FSL_IMX8MP_GPT1,
    FSL_IMX8MP_GPT2,
    FSL_IMX8MP_GPT3,
    FSL_IMX8MP_GPT4,
    FSL_IMX8MP_GPT5,
    FSL_IMX8MP_GPT6,
    FSL_IMX8MP_GPU2D,
    FSL_IMX8MP_GPU3D,
    FSL_IMX8MP_HDMI_TX,
    FSL_IMX8MP_HDMI_TX_AUDLNK_MSTR,
    FSL_IMX8MP_HSIO_BLK_CTL,
    FSL_IMX8MP_I2C1,
    FSL_IMX8MP_I2C2,
    FSL_IMX8MP_I2C3,
    FSL_IMX8MP_I2C4,
    FSL_IMX8MP_I2C5,
    FSL_IMX8MP_I2C6,
    FSL_IMX8MP_INTERCONNECT,
    FSL_IMX8MP_IOMUXC,
    FSL_IMX8MP_IOMUXC_GPR,
    FSL_IMX8MP_IPS_DEWARP,
    FSL_IMX8MP_ISI,
    FSL_IMX8MP_ISP1,
    FSL_IMX8MP_ISP2,
    FSL_IMX8MP_LCDIF1,
    FSL_IMX8MP_LCDIF2,
    FSL_IMX8MP_MEDIA_BLK_CTL,
    FSL_IMX8MP_MIPI_CSI1,
    FSL_IMX8MP_MIPI_CSI2,
    FSL_IMX8MP_MIPI_DSI1,
    FSL_IMX8MP_MU_1_A,
    FSL_IMX8MP_MU_1_B,
    FSL_IMX8MP_MU_2_A,
    FSL_IMX8MP_MU_2_B,
    FSL_IMX8MP_MU_3_A,
    FSL_IMX8MP_MU_3_B,
    FSL_IMX8MP_NPU,
    FSL_IMX8MP_OCOTP_CTRL,
    FSL_IMX8MP_OCRAM,
    FSL_IMX8MP_OCRAM_S,
    FSL_IMX8MP_PCIE1,
    FSL_IMX8MP_PCIE1_MEM,
    FSL_IMX8MP_PCIE_PHY1,
    FSL_IMX8MP_PDM,
    FSL_IMX8MP_PERFMON1,
    FSL_IMX8MP_PERFMON2,
    FSL_IMX8MP_PWM1,
    FSL_IMX8MP_PWM2,
    FSL_IMX8MP_PWM3,
    FSL_IMX8MP_PWM4,
    FSL_IMX8MP_QOSC,
    FSL_IMX8MP_QSPI,
    FSL_IMX8MP_QSPI1_RX_BUFFER,
    FSL_IMX8MP_QSPI1_TX_BUFFER,
    FSL_IMX8MP_QSPI_MEM,
    FSL_IMX8MP_RAM,
    FSL_IMX8MP_RDC,
    FSL_IMX8MP_SAI1,
    FSL_IMX8MP_SAI2,
    FSL_IMX8MP_SAI3,
    FSL_IMX8MP_SAI5,
    FSL_IMX8MP_SAI6,
    FSL_IMX8MP_SAI7,
    FSL_IMX8MP_SDMA1,
    FSL_IMX8MP_SDMA2,
    FSL_IMX8MP_SDMA3,
    FSL_IMX8MP_SEMAPHORE1,
    FSL_IMX8MP_SEMAPHORE2,
    FSL_IMX8MP_SEMAPHORE_HS,
    FSL_IMX8MP_SNVS_HP,
    FSL_IMX8MP_SPBA1,
    FSL_IMX8MP_SPBA2,
    FSL_IMX8MP_SRC,
    FSL_IMX8MP_SYSCNT_CMP,
    FSL_IMX8MP_SYSCNT_CTRL,
    FSL_IMX8MP_SYSCNT_RD,
    FSL_IMX8MP_TCM_DTCM,
    FSL_IMX8MP_TCM_ITCM,
    FSL_IMX8MP_TZASC,
    FSL_IMX8MP_UART1,
    FSL_IMX8MP_UART2,
    FSL_IMX8MP_UART3,
    FSL_IMX8MP_UART4,
    FSL_IMX8MP_USB1,
    FSL_IMX8MP_USB2,
    FSL_IMX8MP_USB1_DEV,
    FSL_IMX8MP_USB2_DEV,
    FSL_IMX8MP_USB1_OTG,
    FSL_IMX8MP_USB2_OTG,
    FSL_IMX8MP_USB1_GLUE,
    FSL_IMX8MP_USB2_GLUE,
    FSL_IMX8MP_USDHC1,
    FSL_IMX8MP_USDHC2,
    FSL_IMX8MP_USDHC3,
    FSL_IMX8MP_VPU,
    FSL_IMX8MP_VPU_BLK_CTRL,
    FSL_IMX8MP_VPU_G1_DECODER,
    FSL_IMX8MP_VPU_G2_DECODER,
    FSL_IMX8MP_VPU_VC8000E_ENCODER,
    FSL_IMX8MP_WDOG1,
    FSL_IMX8MP_WDOG2,
    FSL_IMX8MP_WDOG3,
};

static const struct {
    hwaddr addr;
    size_t size;
    const char *name;
} fsl_imx8mp_memmap[] = {
    [FSL_IMX8MP_RAM] = { FSL_IMX8MP_RAM_START, FSL_IMX8MP_RAM_SIZE_MAX, "ram" },
    [FSL_IMX8MP_DDR_PHY_BROADCAST] = { 0x3dc00000, 4 * MiB, "ddr_phy_broadcast" },
    [FSL_IMX8MP_DDR_PERF_MON] = { 0x3d800000, 4 * MiB, "ddr_perf_mon" },
    [FSL_IMX8MP_DDR_CTL] = { 0x3d400000, 4 * MiB, "ddr_ctl" },
    [FSL_IMX8MP_DDR_BLK_CTRL] = { 0x3d000000, 1 * MiB, "ddr_blk_ctrl" },
    [FSL_IMX8MP_DDR_PHY] = { 0x3c000000, 16 * MiB, "ddr_phy" },
    [FSL_IMX8MP_AUDIO_DSP] = { 0x3b000000, 16 * MiB, "audio_dsp" },
    [FSL_IMX8MP_GIC_DIST] = { 0x38800000, 512 * KiB, "gic_dist" },
    [FSL_IMX8MP_GIC_REDIST] = { 0x38880000, 512 * KiB, "gic_redist" },
    [FSL_IMX8MP_NPU] = { 0x38500000, 2 * MiB, "npu" },
    [FSL_IMX8MP_VPU] = { 0x38340000, 2 * MiB, "vpu" },
    [FSL_IMX8MP_VPU_BLK_CTRL] = { 0x38330000, 2 * MiB, "vpu_blk_ctrl" },
    [FSL_IMX8MP_VPU_VC8000E_ENCODER] = { 0x38320000, 2 * MiB, "vpu_vc8000e_encoder" },
    [FSL_IMX8MP_VPU_G2_DECODER] = { 0x38310000, 2 * MiB, "vpu_g2_decoder" },
    [FSL_IMX8MP_VPU_G1_DECODER] = { 0x38300000, 2 * MiB, "vpu_g1_decoder" },
    [FSL_IMX8MP_USB2_GLUE] = { 0x382f0000, 0x100, "usb2_glue" },
    [FSL_IMX8MP_USB2_OTG] = { 0x3820cc00, 0x100, "usb2_otg" },
    [FSL_IMX8MP_USB2_DEV] = { 0x3820c700, 0x500, "usb2_dev" },
    [FSL_IMX8MP_USB2] = { 0x38200000, 0xc700, "usb2" },
    [FSL_IMX8MP_USB1_GLUE] = { 0x381f0000, 0x100, "usb1_glue" },
    [FSL_IMX8MP_USB1_OTG] = { 0x3810cc00, 0x100, "usb1_otg" },
    [FSL_IMX8MP_USB1_DEV] = { 0x3810c700, 0x500, "usb1_dev" },
    [FSL_IMX8MP_USB1] = { 0x38100000, 0xc700, "usb1" },
    [FSL_IMX8MP_GPU2D] = { 0x38008000, 32 * KiB, "gpu2d" },
    [FSL_IMX8MP_GPU3D] = { 0x38000000, 32 * KiB, "gpu3d" },
    [FSL_IMX8MP_QSPI1_RX_BUFFER] = { 0x34000000, 32 * MiB, "qspi1_rx_buffer" },
    [FSL_IMX8MP_PCIE1] = { 0x33800000, 4 * MiB, "pcie1" },
    [FSL_IMX8MP_QSPI1_TX_BUFFER] = { 0x33008000, 32 * KiB, "qspi1_tx_buffer" },
    [FSL_IMX8MP_APBH_DMA] = { 0x33000000, 32 * KiB, "apbh_dma" },

    /* AIPS-5 Begin */
    [FSL_IMX8MP_MU_3_B] = { 0x30e90000, 64 * KiB, "mu_3_b" },
    [FSL_IMX8MP_MU_3_A] = { 0x30e80000, 64 * KiB, "mu_3_a" },
    [FSL_IMX8MP_MU_2_B] = { 0x30e70000, 64 * KiB, "mu_2_b" },
    [FSL_IMX8MP_MU_2_A] = { 0x30e60000, 64 * KiB, "mu_2_a" },
    [FSL_IMX8MP_EDMA_CHANNELS] = { 0x30e40000, 128 * KiB, "edma_channels" },
    [FSL_IMX8MP_EDMA_MANAGEMENT_PAGE] = { 0x30e30000, 64 * KiB, "edma_management_page" },
    [FSL_IMX8MP_AUDIO_BLK_CTRL] = { 0x30e20000, 64 * KiB, "audio_blk_ctrl" },
    [FSL_IMX8MP_SDMA2] = { 0x30e10000, 64 * KiB, "sdma2" },
    [FSL_IMX8MP_SDMA3] = { 0x30e00000, 64 * KiB, "sdma3" },
    [FSL_IMX8MP_AIPS5_CONFIGURATION] = { 0x30df0000, 64 * KiB, "aips5_configuration" },
    [FSL_IMX8MP_SPBA2] = { 0x30cf0000, 64 * KiB, "spba2" },
    [FSL_IMX8MP_AUDIO_XCVR_RX] = { 0x30cc0000, 64 * KiB, "audio_xcvr_rx" },
    [FSL_IMX8MP_HDMI_TX_AUDLNK_MSTR] = { 0x30cb0000, 64 * KiB, "hdmi_tx_audlnk_mstr" },
    [FSL_IMX8MP_PDM] = { 0x30ca0000, 64 * KiB, "pdm" },
    [FSL_IMX8MP_ASRC] = { 0x30c90000, 64 * KiB, "asrc" },
    [FSL_IMX8MP_SAI7] = { 0x30c80000, 64 * KiB, "sai7" },
    [FSL_IMX8MP_SAI6] = { 0x30c60000, 64 * KiB, "sai6" },
    [FSL_IMX8MP_SAI5] = { 0x30c50000, 64 * KiB, "sai5" },
    [FSL_IMX8MP_SAI3] = { 0x30c30000, 64 * KiB, "sai3" },
    [FSL_IMX8MP_SAI2] = { 0x30c20000, 64 * KiB, "sai2" },
    [FSL_IMX8MP_SAI1] = { 0x30c10000, 64 * KiB, "sai1" },
    /* AIPS-5 End */

    /* AIPS-4 Begin */
    [FSL_IMX8MP_HDMI_TX] = { 0x32fc0000, 128 * KiB, "hdmi_tx" },
    [FSL_IMX8MP_TZASC] = { 0x32f80000, 64 * KiB, "tzasc" },
    [FSL_IMX8MP_HSIO_BLK_CTL] = { 0x32f10000, 64 * KiB, "hsio_blk_ctl" },
    [FSL_IMX8MP_PCIE_PHY1] = { 0x32f00000, 64 * KiB, "pcie_phy1" },
    [FSL_IMX8MP_MEDIA_BLK_CTL] = { 0x32ec0000, 64 * KiB, "media_blk_ctl" },
    [FSL_IMX8MP_LCDIF2] = { 0x32e90000, 64 * KiB, "lcdif2" },
    [FSL_IMX8MP_LCDIF1] = { 0x32e80000, 64 * KiB, "lcdif1" },
    [FSL_IMX8MP_MIPI_DSI1] = { 0x32e60000, 64 * KiB, "mipi_dsi1" },
    [FSL_IMX8MP_MIPI_CSI2] = { 0x32e50000, 64 * KiB, "mipi_csi2" },
    [FSL_IMX8MP_MIPI_CSI1] = { 0x32e40000, 64 * KiB, "mipi_csi1" },
    [FSL_IMX8MP_IPS_DEWARP] = { 0x32e30000, 64 * KiB, "ips_dewarp" },
    [FSL_IMX8MP_ISP2] = { 0x32e20000, 64 * KiB, "isp2" },
    [FSL_IMX8MP_ISP1] = { 0x32e10000, 64 * KiB, "isp1" },
    [FSL_IMX8MP_ISI] = { 0x32e00000, 64 * KiB, "isi" },
    [FSL_IMX8MP_AIPS4_CONFIGURATION] = { 0x32df0000, 64 * KiB, "aips4_configuration" },
    /* AIPS-4 End */

    [FSL_IMX8MP_INTERCONNECT] = { 0x32700000, 1 * MiB, "interconnect" },

    /* AIPS-3 Begin */
    [FSL_IMX8MP_ENET2_TSN] = { 0x30bf0000, 64 * KiB, "enet2_tsn" },
    [FSL_IMX8MP_ENET1] = { 0x30be0000, 64 * KiB, "enet1" },
    [FSL_IMX8MP_SDMA1] = { 0x30bd0000, 64 * KiB, "sdma1" },
    [FSL_IMX8MP_QSPI] = { 0x30bb0000, 64 * KiB, "qspi" },
    [FSL_IMX8MP_USDHC3] = { 0x30b60000, 64 * KiB, "usdhc3" },
    [FSL_IMX8MP_USDHC2] = { 0x30b50000, 64 * KiB, "usdhc2" },
    [FSL_IMX8MP_USDHC1] = { 0x30b40000, 64 * KiB, "usdhc1" },
    [FSL_IMX8MP_I2C6] = { 0x30ae0000, 64 * KiB, "i2c6" },
    [FSL_IMX8MP_I2C5] = { 0x30ad0000, 64 * KiB, "i2c5" },
    [FSL_IMX8MP_SEMAPHORE_HS] = { 0x30ac0000, 64 * KiB, "semaphore_hs" },
    [FSL_IMX8MP_MU_1_B] = { 0x30ab0000, 64 * KiB, "mu_1_b" },
    [FSL_IMX8MP_MU_1_A] = { 0x30aa0000, 64 * KiB, "mu_1_a" },
    [FSL_IMX8MP_AUD_IRQ_STEER] = { 0x30a80000, 64 * KiB, "aud_irq_steer" },
    [FSL_IMX8MP_UART4] = { 0x30a60000, 64 * KiB, "uart4" },
    [FSL_IMX8MP_I2C4] = { 0x30a50000, 64 * KiB, "i2c4" },
    [FSL_IMX8MP_I2C3] = { 0x30a40000, 64 * KiB, "i2c3" },
    [FSL_IMX8MP_I2C2] = { 0x30a30000, 64 * KiB, "i2c2" },
    [FSL_IMX8MP_I2C1] = { 0x30a20000, 64 * KiB, "i2c1" },
    [FSL_IMX8MP_AIPS3_CONFIGURATION] = { 0x309f0000, 64 * KiB, "aips3_configuration" },
    [FSL_IMX8MP_CAAM] = { 0x30900000, 256 * KiB, "caam" },
    [FSL_IMX8MP_SPBA1] = { 0x308f0000, 64 * KiB, "spba1" },
    [FSL_IMX8MP_FLEXCAN2] = { 0x308d0000, 64 * KiB, "flexcan2" },
    [FSL_IMX8MP_FLEXCAN1] = { 0x308c0000, 64 * KiB, "flexcan1" },
    [FSL_IMX8MP_UART2] = { 0x30890000, 64 * KiB, "uart2" },
    [FSL_IMX8MP_UART3] = { 0x30880000, 64 * KiB, "uart3" },
    [FSL_IMX8MP_UART1] = { 0x30860000, 64 * KiB, "uart1" },
    [FSL_IMX8MP_ECSPI3] = { 0x30840000, 64 * KiB, "ecspi3" },
    [FSL_IMX8MP_ECSPI2] = { 0x30830000, 64 * KiB, "ecspi2" },
    [FSL_IMX8MP_ECSPI1] = { 0x30820000, 64 * KiB, "ecspi1" },
    /* AIPS-3 End */

    /* AIPS-2 Begin */
    [FSL_IMX8MP_QOSC] = { 0x307f0000, 64 * KiB, "qosc" },
    [FSL_IMX8MP_PERFMON2] = { 0x307d0000, 64 * KiB, "perfmon2" },
    [FSL_IMX8MP_PERFMON1] = { 0x307c0000, 64 * KiB, "perfmon1" },
    [FSL_IMX8MP_GPT4] = { 0x30700000, 64 * KiB, "gpt4" },
    [FSL_IMX8MP_GPT5] = { 0x306f0000, 64 * KiB, "gpt5" },
    [FSL_IMX8MP_GPT6] = { 0x306e0000, 64 * KiB, "gpt6" },
    [FSL_IMX8MP_SYSCNT_CTRL] = { 0x306c0000, 64 * KiB, "syscnt_ctrl" },
    [FSL_IMX8MP_SYSCNT_CMP] = { 0x306b0000, 64 * KiB, "syscnt_cmp" },
    [FSL_IMX8MP_SYSCNT_RD] = { 0x306a0000, 64 * KiB, "syscnt_rd" },
    [FSL_IMX8MP_PWM4] = { 0x30690000, 64 * KiB, "pwm4" },
    [FSL_IMX8MP_PWM3] = { 0x30680000, 64 * KiB, "pwm3" },
    [FSL_IMX8MP_PWM2] = { 0x30670000, 64 * KiB, "pwm2" },
    [FSL_IMX8MP_PWM1] = { 0x30660000, 64 * KiB, "pwm1" },
    [FSL_IMX8MP_AIPS2_CONFIGURATION] = { 0x305f0000, 64 * KiB, "aips2_configuration" },
    /* AIPS-2 End */

    /* AIPS-1 Begin */
    [FSL_IMX8MP_CSU] = { 0x303e0000, 64 * KiB, "csu" },
    [FSL_IMX8MP_RDC] = { 0x303d0000, 64 * KiB, "rdc" },
    [FSL_IMX8MP_SEMAPHORE2] = { 0x303c0000, 64 * KiB, "semaphore2" },
    [FSL_IMX8MP_SEMAPHORE1] = { 0x303b0000, 64 * KiB, "semaphore1" },
    [FSL_IMX8MP_GPC] = { 0x303a0000, 64 * KiB, "gpc" },
    [FSL_IMX8MP_SRC] = { 0x30390000, 64 * KiB, "src" },
    [FSL_IMX8MP_CCM] = { 0x30380000, 64 * KiB, "ccm" },
    [FSL_IMX8MP_SNVS_HP] = { 0x30370000, 64 * KiB, "snvs_hp" },
    [FSL_IMX8MP_ANA_PLL] = { 0x30360000, 64 * KiB, "ana_pll" },
    [FSL_IMX8MP_OCOTP_CTRL] = { 0x30350000, 64 * KiB, "ocotp_ctrl" },
    [FSL_IMX8MP_IOMUXC_GPR] = { 0x30340000, 64 * KiB, "iomuxc_gpr" },
    [FSL_IMX8MP_IOMUXC] = { 0x30330000, 64 * KiB, "iomuxc" },
    [FSL_IMX8MP_GPT3] = { 0x302f0000, 64 * KiB, "gpt3" },
    [FSL_IMX8MP_GPT2] = { 0x302e0000, 64 * KiB, "gpt2" },
    [FSL_IMX8MP_GPT1] = { 0x302d0000, 64 * KiB, "gpt1" },
    [FSL_IMX8MP_WDOG3] = { 0x302a0000, 64 * KiB, "wdog3" },
    [FSL_IMX8MP_WDOG2] = { 0x30290000, 64 * KiB, "wdog2" },
    [FSL_IMX8MP_WDOG1] = { 0x30280000, 64 * KiB, "wdog1" },
    [FSL_IMX8MP_ANA_OSC] = { 0x30270000, 64 * KiB, "ana_osc" },
    [FSL_IMX8MP_ANA_TSENSOR] = { 0x30260000, 64 * KiB, "ana_tsensor" },
    [FSL_IMX8MP_GPIO5] = { 0x30240000, 64 * KiB, "gpio5" },
    [FSL_IMX8MP_GPIO4] = { 0x30230000, 64 * KiB, "gpio4" },
    [FSL_IMX8MP_GPIO3] = { 0x30220000, 64 * KiB, "gpio3" },
    [FSL_IMX8MP_GPIO2] = { 0x30210000, 64 * KiB, "gpio2" },
    [FSL_IMX8MP_GPIO1] = { 0x30200000, 64 * KiB, "gpio1" },
    [FSL_IMX8MP_AIPS1_CONFIGURATION] = { 0x301f0000, 64 * KiB, "aips1_configuration" },
    /* AIPS-1 End */

    [FSL_IMX8MP_A53_DAP] = { 0x28000000, 16 * MiB, "a53_dap" },
    [FSL_IMX8MP_PCIE1_MEM] = { 0x18000000, 128 * MiB, "pcie1_mem" },
    [FSL_IMX8MP_QSPI_MEM] = { 0x08000000, 256 * MiB, "qspi_mem" },
    [FSL_IMX8MP_OCRAM] = { 0x00900000, 576 * KiB, "ocram" },
    [FSL_IMX8MP_TCM_DTCM] = { 0x00800000, 128 * KiB, "tcm_dtcm" },
    [FSL_IMX8MP_TCM_ITCM] = { 0x007e0000, 128 * KiB, "tcm_itcm" },
    [FSL_IMX8MP_OCRAM_S] = { 0x00180000, 36 * KiB, "ocram_s" },
    [FSL_IMX8MP_CAAM_MEM] = { 0x00100000, 32 * KiB, "caam_mem" },
    [FSL_IMX8MP_BOOT_ROM_PROTECTED] = { 0x0003f000, 4 * KiB, "boot_rom_protected" },
    [FSL_IMX8MP_BOOT_ROM] = { 0x00000000, 252 * KiB, "boot_rom" },
};

enum FslImx8mpIrqs {
    FSL_IMX8MP_USDHC1_IRQ   = 22,
    FSL_IMX8MP_USDHC2_IRQ   = 23,
    FSL_IMX8MP_USDHC3_IRQ   = 24,

    FSL_IMX8MP_UART1_IRQ    = 26,
    FSL_IMX8MP_UART2_IRQ    = 27,
    FSL_IMX8MP_UART3_IRQ    = 28,
    FSL_IMX8MP_UART4_IRQ    = 29,
    FSL_IMX8MP_UART5_IRQ    = 30,
    FSL_IMX8MP_UART6_IRQ    = 16,

    FSL_IMX8MP_ECSPI1_IRQ   = 31,
    FSL_IMX8MP_ECSPI2_IRQ   = 32,
    FSL_IMX8MP_ECSPI3_IRQ   = 33,

    FSL_IMX8MP_I2C1_IRQ     = 35,
    FSL_IMX8MP_I2C2_IRQ     = 36,
    FSL_IMX8MP_I2C3_IRQ     = 37,
    FSL_IMX8MP_I2C4_IRQ     = 38,

    FSL_IMX8MP_USB1_IRQ     = 40,
    FSL_IMX8MP_USB2_IRQ     = 41,

    FSL_IMX8MP_GPT1_IRQ      = 55,
    FSL_IMX8MP_GPT2_IRQ      = 54,
    FSL_IMX8MP_GPT3_IRQ      = 53,
    FSL_IMX8MP_GPT4_IRQ      = 52,
    FSL_IMX8MP_GPT5_GPT6_IRQ = 51,

    FSL_IMX8MP_GPIO1_LOW_IRQ  = 64,
    FSL_IMX8MP_GPIO1_HIGH_IRQ = 65,
    FSL_IMX8MP_GPIO2_LOW_IRQ  = 66,
    FSL_IMX8MP_GPIO2_HIGH_IRQ = 67,
    FSL_IMX8MP_GPIO3_LOW_IRQ  = 68,
    FSL_IMX8MP_GPIO3_HIGH_IRQ = 69,
    FSL_IMX8MP_GPIO4_LOW_IRQ  = 70,
    FSL_IMX8MP_GPIO4_HIGH_IRQ = 71,
    FSL_IMX8MP_GPIO5_LOW_IRQ  = 72,
    FSL_IMX8MP_GPIO5_HIGH_IRQ = 73,

    FSL_IMX8MP_I2C5_IRQ     = 76,
    FSL_IMX8MP_I2C6_IRQ     = 77,

    FSL_IMX8MP_WDOG1_IRQ    = 78,
    FSL_IMX8MP_WDOG2_IRQ    = 79,
    FSL_IMX8MP_WDOG3_IRQ    = 10,

    FSL_IMX8MP_ENET1_MAC_IRQ    = 118,
    FSL_IMX6_ENET1_MAC_1588_IRQ = 121,

    FSL_IMX8MP_PCI_INTA_IRQ = 126,
    FSL_IMX8MP_PCI_INTB_IRQ = 125,
    FSL_IMX8MP_PCI_INTC_IRQ = 124,
    FSL_IMX8MP_PCI_INTD_IRQ = 123,
    FSL_IMX8MP_PCI_MSI_IRQ  = 140,
};

#endif /* FSL_IMX8MP_H */
