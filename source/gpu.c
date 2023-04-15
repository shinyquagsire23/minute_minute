/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "gpu.h"

#include "asic.h"

void* gpu_tv_primary_surface_addr(void) {
    return abif_gpu_read32(D1GRPH_PRIMARY_SURFACE_ADDRESS);
}

void* gpu_drc_primary_surface_addr(void) {
    return abif_gpu_read32(D2GRPH_PRIMARY_SURFACE_ADDRESS);
}

void gpu_test(void) {
#if 0
    for (int i = 0; i < 0x1000; i += 4) {
        printf("%04x: %08x\n", 0x6100 + i, abif_gpu_read32(0x6100 + i)); 
    }
    for (int i = 0; i < 0x80; i += 4) {
        printf("%04x: %08x\n", 0x7100 + i, abif_gpu_read32(0x7100 + i)); 
    }

    // 01200000 when working, 01000004 when JTAG fuses unloaded
    //printf("UVD idk %08x\n", abif_gpu_read32(0x3D57 * 4)); 
#endif
}