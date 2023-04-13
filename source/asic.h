/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _LATTE_ASIC_H
#define _LATTE_ASIC_H

#include "types.h"

// "ABIF's HDP", "Host Datapath"?
// "ABIF's SRBM", "System Register Block Manager"?

int abif_reg_indir_rd_modif_wr(u32 addr, u32 wr_data, u32 size_val, u32 shift_val, u32 post_wr_read, u32 tile_id);
u16 abif_cpl_ct_read32(u32 offset);
u16 abif_cpl_tr_read16(u32 offset);
u16 abif_cpl_tl_read16(u32 offset);
u16 abif_cpl_br_read16(u32 offset);
u16 abif_cpl_bl_read16(u32 offset);
u32 abif_gpu_read32(u32 offset);
void abif_cpl_ct_write32(u32 offset, u32 value32);
void abif_cpl_tr_write16(u32 offset, u16 value16);
void abif_cpl_tl_write16(u32 offset, u16 value16);
void abif_cpl_br_write16(u32 offset, u16 value16);
void abif_cpl_bl_write16(u32 offset, u16 value16);
void abif_gpu_write32(u32 offset, u32 value32);
void abif_gpu_mask32(u32 offset, u32 clear32, u32 set32);

#endif // _LATTE_ASIC_H