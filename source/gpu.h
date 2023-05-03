/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __GPU_H__
#define __GPU_H__

#include "types.h"

//https://android.googlesource.com/kernel/msm/+/android-7.1.0_r0.2/drivers/gpu/drm/radeon

//
// DMCU regs
// from: https://github.com/torvalds/linux/blob/cec24b8b6bb841a19b5c5555b600a511a8988100/drivers/gpu/drm/amd/include/asic_reg/dce/dce_6_0_d.h#LL3012C1-L3039C45
//
#define mmDMCU_CTRL 0x1600
#define mmDMCU_STATUS 0x1601
#define mmDMCU_PC_START_ADDR 0x1602
#define mmDMCU_FW_START_ADDR 0x1603
#define mmDMCU_FW_END_ADDR 0x1604
#define mmDMCU_FW_ISR_START_ADDR 0x1605
#define mmDMCU_FW_CS_HI 0x1606
#define mmDMCU_FW_CS_LO 0x1607
#define mmDMCU_RAM_ACCESS_CTRL 0x1608
#define mmDMCU_ERAM_WR_CTRL 0x1609
#define mmDMCU_ERAM_WR_DATA 0x160A
#define mmDMCU_ERAM_RD_CTRL 0x160B
#define mmDMCU_ERAM_RD_DATA 0x160C
#define mmDMCU_IRAM_WR_CTRL 0x160D
#define mmDMCU_IRAM_WR_DATA 0x160E
#define mmDMCU_IRAM_RD_CTRL 0x160F
#define mmDMCU_IRAM_RD_DATA 0x1610
#define mmDMCU_EVENT_TRIGGER 0x1611
#define mmDMCU_UC_INTERNAL_INT_STATUS 0x1612

#define mmDMCU_INTERRUPT_STATUS 0x1614
#define mmDMCU_INTERRUPT_TO_HOST_EN_MASK 0x1615
#define mmDMCU_INTERRUPT_TO_UC_EN_MASK 0x1616
#define mmDMCU_INTERRUPT_TO_UC_XIRQ_IRQ_SEL 0x1617

#define mmDMCU_INT_CNT 0x1619
#define mmDMCU_FW_CHECKSUM_SMPL_BYTE_POS 0x161A
#define mmDMCU_UC_CLK_GATING_CNTL 0x161B

#define mmDMCU_TEST_DEBUG_INDEX 0x1626
#define mmDMCU_TEST_DEBUG_DATA 0x1627

//
// Other
//
//#define DMCU_RESET  (0x5800) // DMCU deassert = clear bit0
#define DC1_BASE (0x6000)
#define DC2_BASE (0x6800)

#define D1GRPH_BASE (0x6100)
#define D2GRPH_BASE (0x6900)


#define MCIF_CONTROL                     (0x6CB8)
#define DC_LUT_AUTOFILL                  (0x64A0)

// DC1
#define D1CRTC_H_TOTAL                          (DC1_BASE + 0x000)
#define D1CRTC_H_BLANK_START_END                (DC1_BASE + 0x004)
#define D1CRTC_H_SYNC_A                         (DC1_BASE + 0x008)
#define D1CRTC_V_TOTAL                          (DC1_BASE + 0x020)
#define D1CRTC_V_BLANK_START_END                (DC1_BASE + 0x024)
#define D1CRTC_V_SYNC_A                         (DC1_BASE + 0x028)
#define D1CRTC_CONTROL                          (DC1_BASE + 0x080)
#define D1CRTC_INTERLACE_CONTROL                (DC1_BASE + 0x088)
#define D1CRTC_OVERSCAN_COLOR                   (DC1_BASE + 0x094)
#define D1CRTC_BLACK_COLOR                      (DC1_BASE + 0x098)
#define D1MODE_MASTER_UPDATE_LOCK               (DC1_BASE + 0x0E0)
#define D1CRTC_UPDATE_LOCK                      (DC1_BASE + 0x0E8)
#define D1GRPH_ENABLE                           (DC1_BASE + 0x100)
#define D1GRPH_CONTROL                          (DC1_BASE + 0x104)
#define D1GRPH_LUT_SEL                          (DC1_BASE + 0x108)
#define D1GRPH_SWAP_CNTL                        (DC1_BASE + 0x10C)
#define D1GRPH_PRIMARY_SURFACE_ADDRESS          (DC1_BASE + 0x110)
#define D1GRPH_PRIMARY_SURFACE_ADDRESS_HI       (DC1_BASE + 0x114)
#define D1GRPH_SECONDARY_SURFACE_ADDRESS        (DC1_BASE + 0x118)
#define D1GRPH_SECONDARY_SURFACE_ADDRESS_HI     (DC1_BASE + 0x11C)
#define D1GRPH_PITCH                            (DC1_BASE + 0x120)
#define D1GRPH_SURFACE_OFFSET_X                 (DC1_BASE + 0x124)
#define D1GRPH_SURFACE_OFFSET_Y                 (DC1_BASE + 0x128)
#define D1GRPH_X_START                          (DC1_BASE + 0x12C)
#define D1GRPH_Y_START                          (DC1_BASE + 0x130)
#define D1GRPH_X_END                            (DC1_BASE + 0x134)
#define D1GRPH_Y_END                            (DC1_BASE + 0x138)
#define D1OVL_COLOR_MATRIX_TRANSFORMATION_CNTL  (DC1_BASE + 0x140)
#define D1GRPH_UPDATE                           (DC1_BASE + 0x144)
#define D1GRPH_FLIP_CONTROL                     (DC1_BASE + 0x144)
#define D1OVL_ENABLE                            (DC1_BASE + 0x180)
#define D1OVL_UPDATE                            (DC1_BASE + 0x1AC)
#define D1GRPH_ALPHA                            (DC1_BASE + 0x304)
#define D1OVL_ALPHA                             (DC1_BASE + 0x308)
#define D1OVL_ALPHA_CONTROL                     (DC1_BASE + 0x30C)
#define D1GRPH_COLOR_MATRIX_TRANSFORMATION_CNTL (DC1_BASE + 0x380)

// D2GRPH
#define D2GRPH_PRIMARY_SURFACE_ADDRESS   (DC2_BASE + 0x110)
#define D2GRPH_SECONDARY_SURFACE_ADDRESS (DC2_BASE + 0x118)
#define D2GRPH_PITCH                     (DC2_BASE + 0x120)

void* gpu_tv_primary_surface_addr(void);
void* gpu_drc_primary_surface_addr(void);
void gpu_test(void);
void gpu_display_init(void);
void gpu_cleanup(void);
int gpu_idk_upll();

int clocks_ini(const char* key, const char* value);

#endif // __GPU_H__