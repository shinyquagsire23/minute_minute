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
#include "latte.h"
#include "utils.h"
#include "dram.h"
#include "i2c.h"
#include "gfx.h"
#include "gpu_init.h"
#include "pll.h"
#include "minini.h"
#include <string.h>

void* gpu_tv_primary_surface_addr(void) {
    return (void*)(abif_gpu_read32(D1GRPH_PRIMARY_SURFACE_ADDRESS) & ~4);
}

void* gpu_drc_primary_surface_addr(void) {
    return (void*)(abif_gpu_read32(D2GRPH_PRIMARY_SURFACE_ADDRESS) & ~4);
}

void gpu_dump_dc_regs()
{
    for (int i = 0; i < 0x800; i += 4) {
        printf("%04x: %08x\n", 0x6100 + i, abif_gpu_read32(0x6100 + i)); 
    }
    for (int i = 0; i < 0x800; i += 4) {
        printf("%04x: %08x\n", 0x6900 + i, abif_gpu_read32(0x6900 + i)); 
    }
}

void gpu_test(void) {
#if 0
    // 01200000 when working, 01000004 when JTAG fuses unloaded
    //printf("UVD idk %08x\n", abif_gpu_read32(0x3D57 * 4)); 
#endif

    //gpu_dump_dc_regs();
#if 1
    gpu_display_init();
    //gpu_dump_dc_regs();
#endif
}

void gpu_switch_endianness(void) {
    if (read16(MEM_GPU_ENDIANNESS) & 3 == 1)
        return;

    abif_gpu_write32(0x8020, 0xFFFFFFFF);
    abif_gpu_write32(0xe60, 0x820);
    write16(MEM_GPU_ENDIANNESS, 1);
    udelay(100);
    abif_gpu_write32(0xe60, 0);
    abif_gpu_write32(0x8020, 0);
    udelay(100);
}

void gpu_do_init_list(gpu_init_entry_t* paEntries, u32 len) {
    for (int i = 0; i < len; i++) {
        gpu_init_entry_t* entry = &paEntries[i];

        //printf("%04x: %08x\n", entry->addr, abif_gpu_read32(entry->addr));

        if (entry->clear_bits) {
            u32 val = abif_gpu_read32(entry->addr);
            val &= ~entry->clear_bits;
            val |= entry->set_bits;
            abif_gpu_write32(entry->addr, val);
            //abif_gpu_mask32(entry->addr, entry->clear_bits, entry->set_bits);
        }
        else {
            abif_gpu_write32(entry->addr, entry->set_bits);
        }

        /*if (gpu_tv_primary_surface_addr()) {
            printf("%u GPU addr: %08x\n", i, gpu_tv_primary_surface_addr());
        }*/
        

        if (entry->usec_delay) {
            udelay(entry->usec_delay);
        }
    }
}

void gpu_do_ave_list(ave_init_entry_t* paEntries, u32 len) {
    for (int i = 0; i < len; i++) {
        ave_init_entry_t* entry = &paEntries[i];

        u8 tmp[2];
        tmp[0] = entry->reg_idx;
        tmp[1] = entry->value;
        ave_i2c_write(entry->addr, tmp, 2);

        if (entry->usec_delay) {
            udelay(entry->usec_delay);
        }
    }
}

int BSP_60XeDataStreaming_write(int val)
{
    int v4; // r6
    int v5; // r4
    unsigned int v6; // r4
    int v7; // r3
    int v9; // [sp+0h] [bp-18h] BYREF

    v9 = latte_get_hw_version();
    if ( (v9 & 0xF000000) != 0 )
    {
        set32(LT_60XE_CFG, 0x1000);
        v5 = read32(LT_60XE_CFG);
        udelay(10);
        v6 = v5 & ~8u;
        if (val == 1)
            v7 = 0;
        else
            v7 = 8;
        write32(LT_60XE_CFG, (v7 | v6) & ~0x1000);
    }
    else
    {
        return 1024;
    }
    return 0;
}

int gpu_idk_upll()
{
    u32 val_5E0 = read32(LT_RESETS);
    if ( (val_5E0 & 4) == 0 )
    {
        write32(LT_RESETS, val_5E0 | 4);
        udelay(10);
    }

    abif_gpu_mask32(0x5930u, 0x1FF, 0x49);
    abif_gpu_mask32(0x5938u, 0x1FF, 0x49);

    set32(LT_UNK628, 1u);
    return 0;
}

//abifr 0x01000002

void gpu_display_init(void) {
    //pll_spll_write(&spll_cfg_customclock);

    //gpu_switch_endianness();
    ave_i2c_init(400000, 0);

    int needs_ave_init = 1;
    if (gpu_tv_primary_surface_addr()) {
        needs_ave_init = 0;
    }

    //pll_vi1_shutdown_alt();
    //pll_vi2_shutdown();

    pll_vi1_write(&vi1_pllcfg);
    pll_vi2_write(&vi2_pllcfg);

    //gpu_do_init_list(gpu_init_entries_A, NUM_GPU_ENTRIES_A);
    //gpu_do_ave_list(ave_init_entries_A, NUM_AVE_ENTRIES_A);

    //gpu_do_init_list(gpu_init_entries_B, NUM_GPU_ENTRIES_B);
    //gpu_do_ave_list(ave_init_entries_B, NUM_AVE_ENTRIES_B);

    // messes up DRC?
    gpu_do_init_list(gpu_init_entries_C, NUM_GPU_ENTRIES_C);
    gpu_do_ave_list(ave_init_entries_C, NUM_AVE_ENTRIES_C);

    printf("GPU TV addr: %08x\n", gpu_tv_primary_surface_addr());
    printf("GPU DRC addr: %08x\n", gpu_drc_primary_surface_addr());

    abif_gpu_write32(D1GRPH_PRIMARY_SURFACE_ADDRESS, FB_TV_ADDR);
    abif_gpu_write32(D2GRPH_PRIMARY_SURFACE_ADDRESS, FB_DRC_ADDR);

    // HACK: I can't get the endianness swap to work :/
    if ((read16(MEM_GPU_ENDIANNESS) & 3) != 2) {
        abif_gpu_write32(D1GRPH_SWAP_CNTL, 0x222);
    }
    else {
        abif_gpu_write32(D1GRPH_SWAP_CNTL, 0x220);
    }

    pll_upll_init();

    // 0x800~0x84C
    // 0x864~0x89C
    /*for (int i = 0x800; i < 0x84C; i += 4)
    {
        printf("%04x: %08x\n", i, abif_cpl_ct_read32(i));
    }
    for (int i = 0x864; i < 0x89C; i += 4)
    {
        printf("%04x: %08x\n", i, abif_cpl_ct_read32(i));
    }*/
}

void gpu_cleanup()
{
    u64 gpu_freq = pll_calc_frequency(&spll_cfg_customclock);
    u64 gpu_freq_mhz = gpu_freq / 1000000;
    u64 gpu_freq_remainder = (gpu_freq-(gpu_freq_mhz * 1000000));
    printf("GPU clocked at: %llu.%lluMHz\n", gpu_freq_mhz, gpu_freq_remainder);
    pll_spll_write(&spll_cfg_customclock);
    udelay(500);

    abif_gpu_write32(0x60e0, 0x0);
    abif_gpu_write32(0x898, 0xFFFFFFFF);

    // HACK: I can't get the endianness swap to work :/
    if ((read16(MEM_GPU_ENDIANNESS) & 3) != 2)
        abif_gpu_write32(D1GRPH_SWAP_CNTL, 0x220);
}

int clocks_ini(const char* key, const char* value)
{
    if(!strcmp(key, "gpu_clk_r")) {
        u32 val_raw = minini_get_uint(value, 0);
        spll_cfg_customclock.clkR = val_raw & 0xFFFF;
        printf("gpu_clk_r=0x%04x\n", spll_cfg_customclock.clkR);
    }
    else if(!strcmp(key, "gpu_clk_f")) {
        u32 val_raw = minini_get_uint(value, 0x285ED0);
        spll_cfg_customclock.clkFMsb = val_raw >> 16;
        spll_cfg_customclock.clkFLsb = (val_raw & 0xFFFF) >> 1;
        printf("gpu_clk_f_msb=0x%04x\n", spll_cfg_customclock.clkFMsb);
        printf("gpu_clk_f_lsb=0x%04x\n", spll_cfg_customclock.clkFLsb);
    }
    else if(!strcmp(key, "gpu_clk_s")) {
        u32 val_raw = minini_get_uint(value, 0x1C2);
        spll_cfg_customclock.clkS = val_raw & 0xFFFF;
        printf("gpu_clk_s=0x%04x\n", spll_cfg_customclock.clkS);
    }
    else if(!strcmp(key, "gpu_clk_v")) {
        u32 val_raw = minini_get_uint(value, 7);
        spll_cfg_customclock.clkVMsb = val_raw >> 16;
        spll_cfg_customclock.clkVLsb = val_raw & 0xFFFF;
        printf("gpu_clk_v_msb=0x%04x\n", spll_cfg_customclock.clkVMsb);
        printf("gpu_clk_v_lsb=0x%04x\n", spll_cfg_customclock.clkVLsb);
    }
    else if(!strcmp(key, "gpu_clk_o_0div")) {
        u32 val_raw = minini_get_uint(value, 4);
        spll_cfg_customclock.clkO0Div = val_raw & 0xFFFF;
        printf("gpu_clk_o_0div=0x%04x\n", spll_cfg_customclock.clkO0Div);
    }
    else if(!strcmp(key, "gpu_clk_o_1div")) {
        u32 val_raw = minini_get_uint(value, 4);
        spll_cfg_customclock.clkO1Div = val_raw & 0xFFFF;
        printf("gpu_clk_o_1div=0x%04x\n", spll_cfg_customclock.clkO1Div);
    }
    else if(!strcmp(key, "gpu_clk_o_2div")) {
        u32 val_raw = minini_get_uint(value, 0);
        spll_cfg_customclock.clkO2Div = val_raw & 0xFFFF;
        printf("gpu_clk_o_2div=0x%04x\n", spll_cfg_customclock.clkO2Div);
    }

    /*u64 gpu_freq = pll_calc_frequency(&spll_cfg_customclock);
    u64 gpu_freq_mhz = gpu_freq / 1000000;
    u64 gpu_freq_remainder = (gpu_freq-(gpu_freq_mhz * 1000000));
    printf("GPU clocked at: %llu.%lluMHz\n", gpu_freq_mhz, gpu_freq_remainder);*/
    
    return 0;
}