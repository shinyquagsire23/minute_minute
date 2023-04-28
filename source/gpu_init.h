/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __GPU_INIT_H__
#define __GPU_INIT_H__

#include "types.h"

#define NUM_GPU_ENTRIES_A (0x61)
#define NUM_AVE_ENTRIES_A (0x25)
#define NUM_GPU_ENTRIES_B (0x13B)
#define NUM_AVE_ENTRIES_B (0x26)
#define NUM_GPU_ENTRIES_C (0x65) // 61
#define NUM_AVE_ENTRIES_C (0x25)

#define FB_TV_ADDR  (0x14000000 + 0x3500000)
#define FB_DRC_ADDR (0x14000000 + 0x38C0000)

typedef struct {
    u16 addr;
    u32 clear_bits;
    u32 set_bits;
    u32 usec_delay;
} gpu_init_entry_t;

typedef struct {
    u8 addr;
    u8 reg_idx;
    u8 value;
    u32 usec_delay;
} ave_init_entry_t;

extern ave_init_entry_t ave_init_entries_A[];
extern ave_init_entry_t ave_init_entries_B[];
extern ave_init_entry_t ave_init_entries_C[]; // mine

extern gpu_init_entry_t gpu_init_entries_A[];
extern gpu_init_entry_t gpu_init_entries_B[];
extern gpu_init_entry_t gpu_init_entries_C[]; // mine

#endif // __GPU_INIT_H__