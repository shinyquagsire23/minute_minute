/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "asic.h"

#include "utils.h"
#include "latte.h"

// From c2w
int abif_reg_indir_rd_modif_wr(u32 addr, u32 wr_data, u32 size_val, u32 shift_val, u32 post_wr_read, u32 tile_id)
{
    int val = 0;
    int result = 0;

    write32(LT_ABIF_OFFSET, addr | (tile_id << 30));
    if ( size_val != 32 ) {
        val = read32(LT_ABIF_DATA) & ~(((1 << size_val) - 1) << shift_val);
    }

    write32(LT_ABIF_DATA, val | (wr_data << shift_val));
    if (post_wr_read) {
        read32(LT_ABIF_DATA);
    }
    return result;
}

u32 abif_basic_read32(u32 offset)
{
    u32 ret = 0;

    write32(LT_ABIF_OFFSET, offset);
    ret = read32(LT_ABIF_DATA);

    return ret;
}

void abif_basic_write32(u32 offset, u32 val)
{
    write32(LT_ABIF_OFFSET, offset);
    write32(LT_ABIF_DATA, val);
}

static void abif_force_data_read_cycle()
{
    read32(LT_ABIF_DATA);
}

u32 abif_cpl_ct_read32(u32 offset)
{
    u32 ret = 0;

    write32(LT_ABIF_OFFSET, offset);
    ret |= read32(LT_ABIF_DATA) & 0xFFFF;
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) + 2);
    ret |= read32(LT_ABIF_DATA) << 16;

    return ret;
}

u16 abif_cpl_tr_read16(u32 offset)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x01000000);
    return read32(LT_ABIF_DATA) & 0xFFFF;
}

u16 abif_cpl_tl_read16(u32 offset)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x02000000);
    return read32(LT_ABIF_DATA) & 0xFFFF;
}

u16 abif_cpl_br_read16(u32 offset)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x03000000);
    return read32(LT_ABIF_DATA) & 0xFFFF;
}

u16 abif_cpl_bl_read16(u32 offset)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x04000000);
    return read32(LT_ABIF_DATA) & 0xFFFF;
}

u32 abif_gpu_read32(u32 offset)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0xC0000000);
    return read32(LT_ABIF_DATA);
}

void abif_cpl_ct_write32(u32 offset, u32 value32)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF));
    write32(LT_ABIF_DATA, value32 & 0xFFFF);
    abif_force_data_read_cycle();
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) + 2);
    write32(LT_ABIF_DATA, value32>>16);
    abif_force_data_read_cycle();
}

void abif_cpl_tr_write16(u32 offset, u16 value16)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x01000000);
    write32(LT_ABIF_DATA, value16);
    abif_force_data_read_cycle();
}

void abif_cpl_tl_write16(u32 offset, u16 value16)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x02000000);
    write32(LT_ABIF_DATA, value16);
    abif_force_data_read_cycle();
}

void abif_cpl_br_write16(u32 offset, u16 value16)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x03000000);
    write32(LT_ABIF_DATA, value16);
    abif_force_data_read_cycle();
}

void abif_cpl_bl_write16(u32 offset, u16 value16)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0x04000000);
    write32(LT_ABIF_DATA, value16);
    abif_force_data_read_cycle();
}

void abif_gpu_write32(u32 offset, u32 value32)
{
    write32(LT_ABIF_OFFSET, (offset & 0xFFFF) | 0xC0000000);
    write32(LT_ABIF_DATA, value32);
    abif_force_data_read_cycle();
}

void abif_gpu_mask32(u32 offset, u32 clear32, u32 set32)
{
    u32 val = abif_gpu_read32(offset);
    val &= ~clear32;
    val |= set32;
    abif_gpu_write32(offset, val);
}