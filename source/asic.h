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

// 0x00000000 is not valid
// 0x40000000 is not valid
// 0x80000000 is a valid device
// 0xd0000000 is a valid device?
// 0xe0000000 is also the GPU
// 0xf0000000 is also a valid device?

// These seem to mirror tile 0
// 0x41000000 = 4
// 0x42000000 = 0
// 0x43000000 = 0
// 0x44000000 = 0
// 0x45000000 = hang

// Mirror into MEM1?
// 0x80000000 = 20008000
// 0x80000004 = 79fbfdcf
// 0x80000008 = e1ebc3b7
// 0x8000000c = fff5fffd
// 0x80000010 = 7cfdfac7
// 0x81000000 = also memory
// 0x82000014 = 0xdeadbeef, sets off watchdog?
// 0x83000000 = 0xdeadbeef
// 0x84000000 = 0xdeadbeef
// 0x85000000 = 0xdeadbeef
// 0x86000000 = 0xdeadbeef
// 0x87000000 = 0xdeadbeef
// 0x88000000 = 0xdeadbeef
// 0x89000000 = 0xdeadbeef
// 0x8a000000 = 0xdeadbeef
// 0x8b000000 = 0xdeadbeef
// 0x8c000000 = 0xdeadbeef

// 0xc1000000...0xce000000 I think I hit another watchdog

int abif_reg_indir_rd_modif_wr(u32 addr, u32 wr_data, u32 size_val, u32 shift_val, u32 post_wr_read, u32 tile_id);
u32 abif_basic_read32(u32 offset);
void abif_basic_write32(u32 offset, u32 val);
u32 abif_cpl_ct_read32(u32 offset);
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

// AMD uses indices instead of addresses.
inline u32 abif_gpu_read32_idx(u32 offset)
{
    return abif_gpu_read32(offset*4);
}

inline void abif_gpu_write32_idx(u32 offset, u32 value32)
{
    abif_gpu_write32(offset*4, value32);
}

inline void abif_gpu_mask32_idx(u32 offset, u32 clear32, u32 set32)
{
    abif_gpu_mask32(offset*4, clear32, set32);
}

#endif // _LATTE_ASIC_H