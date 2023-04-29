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
#define D1GRPH_BASE (0x6100)
#define D2GRPH_BASE (0x6900)

#define D1MODE_MASTER_UPDATE_LOCK        (0x60E0)
#define MCIF_CONTROL                     (0x6CB8)
#define DC_LUT_AUTOFILL                  (0x64A0)

// D1GRPH
#define D1GRPH_ENABLE                    (D1GRPH_BASE + 0x000)
#define D1GRPH_CONTROL                   (D1GRPH_BASE + 0x004)
#define D1GRPH_LUT_SEL                   (D1GRPH_BASE + 0x008)
#define D1GRPH_SWAP_CNTL                 (D1GRPH_BASE + 0x00C)
#define D1GRPH_PRIMARY_SURFACE_ADDRESS   (D1GRPH_BASE + 0x010)
#define D1GRPH_SECONDARY_SURFACE_ADDRESS (D1GRPH_BASE + 0x014)
#define D1GRPH_PITCH                     (D1GRPH_BASE + 0x020)
#define D1GRPH_SURFACE_OFFSET_X          (D1GRPH_BASE + 0x024)
#define D1GRPH_SURFACE_OFFSET_Y          (D1GRPH_BASE + 0x028)
#define D1GRPH_X_START                   (D1GRPH_BASE + 0x02C)
#define D1GRPH_Y_START                   (D1GRPH_BASE + 0x030)
#define D1GRPH_X_END                     (D1GRPH_BASE + 0x034)
#define D1GRPH_Y_END                     (D1GRPH_BASE + 0x038)
#define D1OVL_COLOR_MATRIX_TRANSFORMATION_CNTL (D1GRPH_BASE + 0x040)
#define D1GRPH_UPDATE                    (D1GRPH_BASE + 0x044)
#define D1GRPH_FLIP_CONTROL              (D1GRPH_BASE + 0x044)
#define D1OVL_ENABLE                     (D1GRPH_BASE + 0x080)
#define D1OVL_UPDATE                     (D1GRPH_BASE + 0x0AC)
#define D1GRPH_ALPHA                     (D1GRPH_BASE + 0x204)
#define D1OVL_ALPHA                      (D1GRPH_BASE + 0x208)
#define D1OVL_ALPHA_CONTROL              (D1GRPH_BASE + 0x20C)
#define D1GRPH_COLOR_MATRIX_TRANSFORMATION_CNTL (D1GRPH_BASE + 0x280)

// D2GRPH
#define D2GRPH_PRIMARY_SURFACE_ADDRESS   (D2GRPH_BASE + 0x010)
#define D2GRPH_SECONDARY_SURFACE_ADDRESS (D2GRPH_BASE + 0x014)
#define D2GRPH_PITCH                     (D2GRPH_BASE + 0x020)

void* gpu_tv_primary_surface_addr(void);
void* gpu_drc_primary_surface_addr(void);
void gpu_test(void);
void gpu_display_init(void);
void gpu_cleanup(void);

#endif // __GPU_H__