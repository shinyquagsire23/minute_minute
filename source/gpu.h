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

#define DMCU_RESET  (0x5800) // DMCU deassert = clear bit0
#define D1GRPH_BASE (0x6100)
#define D2GRPH_BASE (0x6900)

// D1GRPH
#define D1GRPH_PRIMARY_SURFACE_ADDRESS (D1GRPH_BASE + 0x010)

// D2GRPH
#define D2GRPH_PRIMARY_SURFACE_ADDRESS (D2GRPH_BASE + 0x010)

void* gpu_tv_primary_surface_addr(void);
void* gpu_drc_primary_surface_addr(void);
void gpu_test(void);

#endif // __GPU_H__