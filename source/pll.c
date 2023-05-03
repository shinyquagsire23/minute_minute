/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "pll.h"

#include "asic.h"
#include "utils.h"
#include "latte.h"
#include "gpu.h"
#include "crypto.h"
#include <string.h>

// div_select = ?
// clkO = div_select == 0 ? clkO0Div : div_select == 1 ? clkO1Div : clkO2Div
// clkF = (clkFMsb << 16) | (clkFLsb << 1)
// freqMhz = 27 * (clkF/0x10000) / (clkR+1) / (clkO/2)
// clkV is spread spectrum related maybe?
// clkS is clock source...?

// calculations:
// 27 * (0x372000 / 0x10000) / (0+1) / (0xc/2)
// 27 * (0x352000 / 0x10000) / (0+1) / (0xc/2)
// 27 * (0x360000 / 0x10000) / (0+1) / (0xc/2)
// 27 * (0x3B0000 / 0x10000) / (0+1) / (0x2/2)
// 27 * (0x350000 / 0x10000) / (0+1) / (0x2/2)
// 27 * (0x400000 / 0x10000) / (0+1) / (0x4/2)
// 27 * (0x285ED0 / 0x10000) / (0+1) / (0x4/2)
// 27 * (0x1F40000 / 0x10000) / (0xC+1) / (0xC/2)
// 27 * (0x2C0000 / 0x10000) / (0+1) / (0x2C/2)
// 27 * (0x480000 / 0x10000) / (0+1) / (0x8/2)
// 27 * (0x3F0000 / 0x10000) / (0+1) / (0x48/2)
// 27 * (0x320000 / 0x10000) / (0+1) / (0x36/2)
// 27 * (0x320000 / 0x10000) / (0+1) / (0x36/2)

// clock rates:
// syspll_248_cfg   = 248.0625MHz
// syspll_240_cfg   = 239.0625MHz
// syspll_243_cfg   = 243MHz
// dram_1_pllcfg    = 1593MHz
// dram_2_pllcfg    = 1431MHz
// dram_3_pllcfg    = 864MHz
// spll_cfg         = 544.9998MHz (AKA GFX pll)
// vi1_pllcfg       = 173.0769MHz?
// vi2_pllcfg       = 54MHz?
// upll_cfg         = 486MHz (also GFX?)
// usbpll_cfg       = 47.25MHz
// sata_1_pllcfg    = 50MHz
// sata_2_pllcfg    = 50MHz

// TR: vi1, vi2, upll...?
// BR: DRAM
// TL: syspll
// BL: usb, sata
// CT: upll, spll, 

//                            fastEn dithEn satEn ssEn bypOut bypVco operational clkR clkFMsb clkFLsb clkO0Div clkO1Div clkO2Div clkS   clkVMsb clkVLsb bwAdj options
bsp_pll_cfg syspll_248_cfg  = {0,    1,     1,    0,   0,     0,     0,          0,   0x37,   0x1000, 0xC,     0x18,    0,       0x1C2, 0,      0xA,    0x5,  0x0};
bsp_pll_cfg syspll_240_cfg  = {0,    1,     1,    0,   0,     0,     0,          0,   0x35,   0x1000, 0xC,     0x18,    0,       0x1C2, 0,      0x9,    0x5,  0x0};
bsp_pll_cfg syspll_243_cfg  = {0,    1,     1,    0,   0,     0,     0,          0,   0x36,   0x0000, 0xC,     0x18,    0,       0x1C2, 0,      0x9,    0x5,  0x0};
bsp_pll_cfg dram_1_pllcfg   = {0,    1,     1,    0,   1,     0,     1,          0,   0x3B,   0,      0x2,     0x0,     0,       0x1C2, 0,      0xA,    0x6,  0x0};
bsp_pll_cfg dram_2_pllcfg   = {0,    1,     1,    0,   1,     0,     1,          0,   0x35,   0,      0x2,     0x0,     0,       0x1C2, 0,      0xA,    0x5,  0x0};
bsp_pll_cfg dram_3_pllcfg   = {0,    1,     1,    0,   0,     0,     1,          0,   0x40,   0,      0x4,     0x0,     0,       0x1C2, 0,      0xB,    0x7,  0x0};
bsp_pll_cfg spll_cfg        = {0,    1,     1,    1,   0,     0,     0,          0,   0x28,   0x2F68, 0x4,     0x4,     0,       0x1C2, 0,      0x7,    0x4,  0x0};
bsp_pll_cfg vi1_pllcfg      = {0,    1,     1,    0,   0,     0,     0,          0xC, 0x1F4,  0,      0xC,     0xC,     0xC,     0,     0,      0x0,    0x3D, 0x1C}; 
bsp_pll_cfg vi2_pllcfg      = {0,    1,     1,    0,   0,     0,     0,          0,   0x2C,   0,      0x2C,    0x4,     0x2C,    0,     0,      0x0,    0x4,  0x0};
bsp_pll_cfg upll_cfg        = {0,    1,     1,    0,   0,     0,     1,          0,   0x48,   0,      0x8,     0xA,     0x54,    0x1C2, 0,      0xD,    0x8,  0x0};
bsp_pll_cfg usbpll_cfg      = {0,    0,     0,    0,   0,     0,     1,          0,   0x3F,   0,      0x48,    0x120,   0x20,    0,     0,      0x0,    0x3,  0x0};
bsp_pll_cfg sata_1_pllcfg   = {0,    1,     0,    0,   0,     0,     1,          0,   0x32,   0,      0x36,    0,       0,       0x1B3, 0,      0x7,    0x4,  0x0};
bsp_pll_cfg sata_2_pllcfg   = {0,    1,     0,    1,   0,     0,     1,          0,   0x32,   0,      0x36,    0,       0,       0x1B3, 0,      0x7,    0x4,  0x0};

bsp_pll_cfg spll_cfg_customclock = {0,1,    1,    1,   0,     0,     0,          0,   0x28,   0x2F68, 0x4,     0x4,     0,       0x1C2, 0,      0x7,    0x4,  0x0};
bsp_pll_cfg spll_cfg_underclock  = {0,1,    1,    1,   0,     0,     0,          0,   0xA,   0x2F68, 0x4,     0x4,     0,       0x1C2, 0,      0x7,    0x4,  0x0};
bsp_pll_cfg spll_cfg_overclock   = {0,1,    1,    1,   0,     0,     0,          0,   0x32,   0x2F68, 0x4,     0x4,     0,       0x1C2, 0,      0x7,    0x4,  0x0};

u64 pll_calc_frequency(bsp_pll_cfg* pCfg)
{
    u64 clkF = (pCfg->clkFMsb << 16 | ((pCfg->clkFLsb & 0x7FFF) << 1));
    u64 clkR = pCfg->clkR;
    u64 clkO = pCfg->clkO0Div; // TODO?

    return (((27000000 * clkF) / (clkR+1)) / (clkO/2)) / 0x10000;
}

int pll_vi1_shutdown()
{
    u16 v0;

    v0 = abif_cpl_tr_read16(0x14);
    if (v0 & 0x8000)
    {
        v0 &= ~0x8000;
        abif_cpl_tr_write16(0x14, v0);
        udelay(5);
    }
    abif_cpl_tr_write16(0x14, v0 & 0xBFFF);
    //abif_cpl_tr_write16(2, 0);
    return 0;
}

int pll_vi1_shutdown_alt()
{
    u16 v0;

    v0 = abif_cpl_tr_read16(0x14);
    if (v0 & 0x8000)
    {
        v0 &= ~0x8000;
        abif_cpl_tr_write16(0x14, v0);
        udelay(5);
    }
    abif_cpl_tr_write16(0x14, v0 & 0xBFFF);
    abif_cpl_tr_write16(2, 0);
    return 0;
}

int pll_vi2_shutdown()
{
    u16 v0;

    v0 = abif_cpl_tr_read16(0x34);
    if (v0 & 0x8000)
    {
        v0 &= ~0x8000;
        abif_cpl_tr_write16(0x34, v0);
        udelay(5);
    }
    abif_cpl_tr_write16(0x34, v0 & 0xBFFF);
    return 0;
}

int pll_vi1_write(bsp_pll_cfg *pCfg)
{
    u16 v4; // r1
    u16 v5; // r3
    u16 v7; // r1
    u16 v8; // r3
    u16 v10; // r1
    u16 v11; // r3
    u16 v13; // r1
    u16 v14; // r3
    u16 v16; // r1
    u32 v17; // r3

    //pll_vi1_shutdown(); // TODO: remove the 2 write?
    udelay(10);
    abif_cpl_tr_write16(2, pCfg->options & 0x1E); // 0x1C

    abif_cpl_tr_write16(0x10, pCfg->clkR | 0x8000 | (u16)(pCfg->clkO0Div << 6)); // 0x30c

    v4 = pCfg->bypVco ? 0x1000 : 0;
    v5 = pCfg->bypOut ? 0x2000 : 0;
    abif_cpl_tr_write16(0x12, (u16)v4 | (u16)(v5 | pCfg->clkFMsb)); // 0x1F4

    v7 = pCfg->satEn ? 0x800 : 0;
    v8 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_tr_write16(0x14, (u16)v7 | (u16)(v8 | pCfg->clkO2Div)); // 0000c80c
    abif_cpl_tr_write16(0x16, pCfg->clkO1Div); // 0000000c
    abif_cpl_tr_write16(0x18, pCfg->clkVLsb); // 00000000

    v10 = pCfg->ssEn ? 0x400 : 0;
    v11 = pCfg->dithEn ? 0x800 : 0;
    abif_cpl_tr_write16(0x1A, (u16)v10 | (u16)(v11 | pCfg->clkVMsb)); // 00000800
    abif_cpl_tr_write16(0x1C, pCfg->clkS); // 00000000
    abif_cpl_tr_write16(0x1E, pCfg->bwAdj); // 0000003d
    abif_cpl_tr_write16(0x20, pCfg->clkFLsb); // 0
    udelay(5);

    v13 = pCfg->satEn ? 0x4800 : 0x4000;
    v14 = pCfg->fastEn ? 0x1000 : 0x0;
    abif_cpl_tr_write16(0x14, (u16)v13 | (u16)(v14 | pCfg->clkO2Div));
    udelay(50);
    
    v16 = pCfg->satEn ? 0xC800 : 0xC000;
    v17 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_tr_write16(0x14, (u16)v16 | (u16)(v17 | pCfg->clkO2Div));
    udelay(50);

    return 0;
}

int pll_vi2_write(bsp_pll_cfg *pCfg)
{
    u16 v4; // r1
    u16 v5; // r3
    u16 v7; // r1
    u16 v8; // r3
    u16 v10; // r1
    u16 v11; // r3
    u16 v13; // r1
    u16 v14; // r3
    u16 v16; // r1
    u32 v17; // r3

    //pll_vi2_shutdown();
    udelay(10);

    abif_cpl_tr_write16(0x30, pCfg->clkR | 0x8000 | (u16)(pCfg->clkO0Div << 6)); // 00000b00

    v4 = pCfg->bypVco ? 0x1000 : 0;
    v5 = pCfg->bypOut ? 0x2000 : 0;
    abif_cpl_tr_write16(0x32, (u16)v4 | (u16)(v5 | pCfg->clkFMsb)); // 0000002c
    
    v7 = pCfg->satEn ? 0x800 : 0;
    v8 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_tr_write16(0x34, (u16)v7 | (u16)(v8 | pCfg->clkO2Div)); // 0000c82c
    abif_cpl_tr_write16(0x36, pCfg->clkO1Div); // 4
    abif_cpl_tr_write16(0x24, pCfg->clkVLsb); // 0

    v10 = pCfg->ssEn ? 0x400 : 0;
    v11 = pCfg->dithEn ? 0x800 : 0;
    abif_cpl_tr_write16(0x26, (u16)v10 | (u16)(v11 | pCfg->clkVMsb)); // 0x800
    abif_cpl_tr_write16(0x28, pCfg->clkS); // 0
    abif_cpl_tr_write16(0x2A, pCfg->bwAdj); // 4
    abif_cpl_tr_write16(0x2C, pCfg->clkFLsb); // 0
    udelay(5);

    v13 = pCfg->satEn ? 0x4800 : 0x4000;
    v14 = pCfg->fastEn ? 0x1000 : 0x0;
    abif_cpl_tr_write16(0x34, (u16)v13 | (u16)(v14 | pCfg->clkO2Div)); // 0000c82c
    udelay(50);
    
    v16 = pCfg->satEn ? 0xC800 : 0xC000;
    v17 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_tr_write16(0x34, (u16)v16 | (u16)(v17 | pCfg->clkO2Div));
    udelay(50);

    return 0;
}

int pll_upll_write(bsp_pll_cfg *pCfg)
{
    unsigned int v2; // lr
    unsigned int v3; // r1
    unsigned int v4; // r12
    unsigned int v5; // r1
    unsigned int v6; // lr
    unsigned int v7; // r1

    abif_cpl_ct_write32(0x888u, 0x2001u);
    abif_cpl_ct_write32(0x878u, pCfg->clkR | (pCfg->clkFMsb << 6) | 0x18000000 | (pCfg->clkO0Div << 18));
    udelay(5);
    abif_cpl_ct_write32(0x878u, pCfg->clkR | (pCfg->clkFMsb << 6) | 0x98000000 | (pCfg->clkO0Div << 18));
    udelay(10);
    abif_cpl_tr_write16(0, pCfg->clkO2Div);
    v2 = pCfg->satEn ? 0x8000000 : 0;
    v3 = pCfg->fastEn ? 0x10000000 : 0;
    abif_cpl_ct_write32(0x87Cu, v2 | v3 | pCfg->clkO1Div | (pCfg->clkFLsb << 9));
    v4 = pCfg->ssEn ? 0x4000000 : 0;
    v5 = pCfg->dithEn ? 0x8000000 : 0;
    abif_cpl_ct_write32(0x880u, v4 | v5 | pCfg->clkVLsb | (pCfg->clkVMsb << 16));
    abif_cpl_ct_write32(0x884u, pCfg->clkS | (pCfg->bwAdj << 12));
    udelay(5);
    abif_cpl_ct_write32(0x878u, pCfg->clkR | (pCfg->clkFMsb << 6) | 0x18000000 | (pCfg->clkO0Div << 18));
    udelay(200);
    v6 = pCfg->bypVco ? 0x8000000 : 0;
    v7 = pCfg->bypOut ? 0x10000000 : 0;
    abif_cpl_ct_write32(0x878u, v6 | v7 | pCfg->clkR | (pCfg->clkFMsb << 6) | (pCfg->clkO0Div << 18));
    abif_cpl_ct_write32(0x888u, 0x4002u);

    return 0;
}

int pll_upll_read(bsp_pll_cfg *pOut)
{
    int v4; // r0
    int v5; // r2
    int v6; // r0
    unsigned int v7; // r2
    int v8; // r3
    unsigned int v9; // r1
    unsigned int v10; // r0
    int v11; // r0
    unsigned int v12; // r3
    unsigned int v13; // r1
    unsigned int v14; // r12
    int v15; // r0
    int v17; // r0
    int result; // r0

    memset(pOut, 0, sizeof(bsp_pll_cfg));

    v4 = abif_cpl_ct_read32(0x878u);
    pOut->clkR = v4 & 0x3F;
    pOut->bypVco = !!(v4 & 0x8000000u);
    pOut->bypOut = !!(v4 & 0x10000000u);
    pOut->clkFMsb = (v4 >> 6) & 0x3FFF;
    pOut->clkO0Div = (v4 >> 18) & 0x1FF;
    v6 = abif_cpl_ct_read32(0x87Cu);
    pOut->clkFLsb = (v6 >> 9) & 0x3FFF;
    pOut->clkO1Div = v6 & 0x1FF;
    pOut->satEn = !!(v6 & 0x8000000);
    pOut->fastEn = !!(v6 & 0x10000000);
    v11 = abif_cpl_ct_read32(0x880u);
    pOut->clkVMsb = (v11 >> 16) & 0x3FFF;
    pOut->clkVLsb = v11 & 0xFFFF;
    pOut->ssEn = !!(v11 & 0x4000000u);
    pOut->dithEn = !!(v11 & 0x8000000);
    v15 = abif_cpl_ct_read32(0x884u);
    pOut->bwAdj = (v15 & 0xFFF000u) >> 12;
    pOut->clkO2Div = abif_cpl_tr_read16(0) & 0x1FF;
    v17 = abif_cpl_ct_read32(0x878u);
    if ( (v17 & 0x20000000) != 0
        || (v17 & 0x10000000) != 0
        || (v17 & 0x8000000u) >> 27
        || (abif_cpl_ct_read32(0x888u) & 0xE000) != 0x4000 && (abif_cpl_ct_read32(0x888u) & 7) != 2 )
    {
        return 0;
    }

    pOut->operational = 1;
    return 0;
}

void pll_upll_init()
{
    /*int v0; // r5
    int v2; // r4
    int v3; // r0
    bsp_pll_cfg cfg; // [sp+2h] [bp-46h] BYREF
    int dram_operational; // [sp+38h] [bp-10h] BYREF

    v0 = BSP_check_dram_operational(&dram_operational);
    if ( !v0 )
    {
        if ( dram_operational == 2 )
        {
            v2 = BSP_vi1_pll_shutdown();
            v3 = BSP_vi2_pll_shutdown();
            MEMORY[0xD8005E0] &= ~4u;
            return v2 | v3;
        }
        else
        {
            bsp_memset(&cfg.fastEn, 0, 0x36u);
            v0 = BSP_gpu_related();
            BSP_upll_read(0, 0, &cfg);
            if ( BSP_memcmp(&BSP_upll_config, &cfg, 54) )
                return v0 | BSP_upll_write_(&BSP_upll_config);
        }
    }
    return v0;*/

    bsp_pll_cfg cfg;
    memset(&cfg, 0, sizeof(bsp_pll_cfg));
    gpu_idk_upll();
    pll_upll_read(&cfg);
    if (memcmp(&upll_cfg, &cfg, sizeof(bsp_pll_cfg))) {
        pll_upll_write(&upll_cfg);
    }
}

int pll_syspll_read(bsp_pll_cfg **ppCfg, u32 *pSysClkFreq)
{
    bsp_pll_cfg *pCfg = NULL;
    u32 freq = 0;
    int result = 0;

    u32 bspVer = latte_get_hw_version();
    if (!bspVer)
        return -1;

    if ( (seeprom.bc.library_version) <= 2u )
    {
        pCfg = &syspll_243_cfg;
        freq = 243000000;
    }
    else if ( seeprom.bc.sys_pll_speed == 0xF0 )
    {
        pCfg = &syspll_240_cfg;
        freq = 239625000;
    }
    else if ( seeprom.bc.sys_pll_speed == 0xF8 )
    {
        pCfg = &syspll_248_cfg;
        freq = 248625000;
    }
    else
    {
        pCfg = &syspll_243_cfg;
        freq = 243000000;
    }

    if ( ppCfg )
        *ppCfg = pCfg;

    if ( pSysClkFreq )
        *pSysClkFreq = freq;
    return result;
}

int pll_syspll_write(bsp_pll_cfg *pParams)
{
    int v1; // lr
    u16 v3; // r0
    u16 v4; // r1
    u16 v5; // r2
    u16 v6; // r0
    u16 v7; // r1
    u16 v8; // r2
    u16 v9; // r0
    u16 v10; // r1
    u16 v11; // r2
    int v13; // [sp+0h] [bp-2Ch]
    int v17; // [sp+2Ch] [bp+0h] BYREF
    u32 bspVer;

    v13 = 0;
    bspVer = latte_get_hw_version();
    if ( bspVer && (bspVer & 0xF000000) != 0 )
    {
        set32(LT_CLOCKINFO, 1);
        udelay(10);
        clear32(LT_RESETS_COMPAT, RSTB_DSKPLL);
        udelay(10);
        clear32(LT_RESETS_COMPAT, NLCKB_SYSPLL);
        udelay(20);
        clear32(LT_RESETS_COMPAT, RSTB_SYSPLL);
        abif_cpl_tl_write16(0x20u, pParams->clkR | (pParams->clkO0Div << 6));
        v3 = pParams->clkFMsb;
        if (pParams->bypVco)
            v4 = 0x1000;
        else
            v4 = 0;
        if (pParams->bypOut)
            v5 = 0x2000;
        else
            v5 = 0;
        abif_cpl_tl_write16(0x22u, v4 | v3 | v5);
        v6 = pParams->clkO1Div;
        if (pParams->satEn)
            v7 = 0x800;
        else
            v7 = 0;
        if (pParams->fastEn)
            v8 = 0x1000;
        else
            v8 = 0;
        abif_cpl_tl_write16(0x24u, v7 | v6 | v8);
        abif_cpl_tl_write16(0x26u, pParams->clkFLsb);
        abif_cpl_tl_write16(0x28u, pParams->clkVLsb);
        v9 = pParams->clkVMsb;
        if ( pParams->ssEn)
            v10 = 0x800;
        else
            v10 = 0;
        if (pParams->dithEn)
            v11 = 0x1000;
        else
            v11 = 0;
        abif_cpl_tl_write16(0x2Au, v10 | v9 | v11);
        abif_cpl_tl_write16(0x2Cu, pParams->clkS);
        abif_cpl_tl_write16(0x2Eu, pParams->bwAdj);
        clear32(LT_CLOCKINFO, 2);
        set32(LT_SYSPLL_CFG, pParams->options & 1);
        udelay(5);
        set32(LT_RESETS_COMPAT, RSTB_SYSPLL);
        udelay(200);
        set32(LT_RESETS_COMPAT, NLCKB_SYSPLL);
        set32(LT_RESETS_COMPAT, RSTB_DSKPLL);
        udelay(200);
        clear32(LT_CLOCKINFO, 1);
    }
    return v13;
}

int pll_syspll_init(int bIdk)
{
    int result = 0;
    bsp_pll_cfg cfg;
    bsp_pll_cfg *ppCfg;

    ppCfg = 0;
    result = pll_syspll_read(&ppCfg, 0);
    if ( !result )
    {
        memcpy(&cfg, ppCfg, sizeof(cfg));
        if ( bIdk )
            cfg.options |= 1;
        result = pll_syspll_write(&cfg);
    }
    return result;
}

int pll_spll_write(bsp_pll_cfg *pPllCfg)
{
    int v3; // r1
    int v4; // r2
    int v5; // r1
    int v6; // r2
    int v7; // r1
    int v8; // r0

    gpu_idk_upll();
    abif_cpl_ct_write32(0x874u, 1);
    abif_cpl_ct_write32(0x864u, (pPllCfg->clkO0Div << 18) | pPllCfg->clkR | (pPllCfg->clkFMsb << 6) | 0x18000000);
    udelay(5);
    abif_cpl_ct_write32(0x864u, (pPllCfg->clkO0Div << 18) | (pPllCfg->clkR) | (pPllCfg->clkFMsb << 6) | 0x98000000);
    udelay(10);

    if ( pPllCfg->satEn )
        v3 = 0x8000000;
    else
        v3 = 0;

    if ( pPllCfg->fastEn )
        v4 = 0x10000000;
    else
        v4 = 0;

    abif_cpl_ct_write32(0x868u, v3 | (pPllCfg->clkFLsb << 9) | pPllCfg->clkO1Div | v4);
    if ( pPllCfg->ssEn )
        v5 = 0x4000000;
    else
        v5 = 0;

    if ( pPllCfg->dithEn )
        v6 = 0x8000000;
    else
        v6 = 0;

    abif_cpl_ct_write32(0x86Cu, v5 | (pPllCfg->clkVMsb << 16) | pPllCfg->clkVLsb | v6);
    abif_cpl_ct_write32(0x870u, (pPllCfg->clkS) | (pPllCfg->bwAdj << 12));
    udelay(5);
    abif_cpl_ct_write32(0x864u, (pPllCfg->clkO0Div << 18) | (pPllCfg->clkR) | (pPllCfg->clkFMsb << 6) | 0x18000000);
    udelay(200);
    if ( pPllCfg->bypVco )
        v7 = 0x8000000;
    else
        v7 = 0;
    if ( pPllCfg->bypOut )
        v8 = 0x10000000;
    else
        v8 = 0;
    abif_cpl_ct_write32(0x864u, v7 | (pPllCfg->clkFMsb << 6) | (pPllCfg->clkR) | (pPllCfg->clkO0Div << 18) | v8);
    abif_cpl_ct_write32(0x874u, 2);
    abif_cpl_ct_write32(0x7D4u, ~0x360000u);
    abif_gpu_write32(0xF4B0u, 1);
    abif_gpu_write32(0xF4A8u, 0x3FFFF);
    return 0;
}

int pll_dram_read(bsp_pll_cfg *pOut)
{
    u16 v2; // r0
    int v8; // r0
    u16 v11; // r0
    int v12; // r2
    int v13; // r0
    u16 v14; // r0
    int v17; // r0

    memset(pOut, 0, sizeof(bsp_pll_cfg));
    pOut->clkVLsb = abif_cpl_br_read16(0x10);

    v2 = abif_cpl_br_read16(0x12);
    pOut->clkVMsb = (v2 & 0x3FFu);
    pOut->ssEn = !!(v2 & 0x400);
    pOut->dithEn = !!(v2 & 0x800);

    pOut->clkS = abif_cpl_br_read16(0x14) & 0xFFF;
    pOut->bwAdj = abif_cpl_br_read16(0x16) & 0xFFF;

    pOut->clkFLsb = abif_cpl_br_read16(0x18) & 0x3FFF;
    v8 = abif_cpl_br_read16(0x1C);
    pOut->clkR = v8 & 0x3F;
    pOut->clkO0Div = (unsigned int)(v8 << 17) >> 23;
    v11 = abif_cpl_br_read16(0x1Eu);
    pOut->clkFMsb = (v11 & 0xFFFu);
    v12 = (u16)(v11 & 0x2000) >> 13;
    v13 = (u16)(v11 & 0x1000) >> 12;
    pOut->bypOut = v12 & 0xFF;
    pOut->bypVco = v13 & 0xFF;
    v14 = abif_cpl_br_read16(0x20u);
    pOut->satEn = !!(v14 & 0x800);
    pOut->fastEn = !!(v14 & 0x1000);

    v17 = abif_cpl_br_read16(0x20u);
    if ( (v17 & 0x4000) != 0 && (unsigned int)(v17 << 16) >> 31 == 1 )
    {
        pOut->operational = 1;
    }
    return 0;
}

int pll_dram_write(bsp_pll_cfg *pCfg)
{
    u16 v2; // r1
    u16 v3; // r3
    u16 v5; // r1
    u16 v6; // r2
    u16 v8; // r1
    u16 v9; // r2
    u16 v10; // r1
    u16 v11; // r3
    u16 v12; // r1
    u16 v13; // r2
    u16 v14; // r1
    u16 v15; // r2

    v2 = pCfg->satEn ? 0x800 : 0;
    v3 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_br_write16(0x20u, v2 | v3);
    udelay(5000);
    abif_cpl_br_write16(0x10, pCfg->clkVLsb);
    
    v5 = pCfg->ssEn ? 0x400 : 0;
    v6 = pCfg->dithEn ? 0x800 : 0;
    abif_cpl_br_write16(0x12, v5 | pCfg->clkVMsb | v6);
    abif_cpl_br_write16(0x14, pCfg->clkS);
    abif_cpl_br_write16(0x16, pCfg->bwAdj);
    abif_cpl_br_write16(0x18, pCfg->clkFLsb);
    abif_cpl_br_write16(0x1C, (pCfg->clkO0Div << 6) | pCfg->clkR | 0x8000);
    
    v8 = pCfg->bypVco ? 0x1000 : 0;
    v9 = pCfg->bypOut ? 0x2000 : 0;
    abif_cpl_br_write16(0x1Eu, v8 | pCfg->clkFMsb | v9);
    
    v10 = pCfg->satEn ? 0x800 : 0;
    v11 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_br_write16(0x20u, v10 | v11);
    udelay(5);
    
    v12 = pCfg->satEn ? 0x4800 : 0x4000;
    v13 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_br_write16(0x20u, v12 | v13);
    udelay(5000);
    
    v14 = pCfg->satEn ? 0xC800 : 0xC000;
    v15 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_br_write16(0x20u, v14 | v15);

    udelay(5000);
    return 0;
}

int pll_usb_read(bsp_pll_cfg *pOut)
{
    u16 v5; // r0
    u16 v6; // r0
    int v7; // r3
    int v9; // r0
    u16 v13; // r0

    memset(pOut, 0, sizeof(bsp_pll_cfg));

    pOut->clkO2Div = abif_cpl_bl_read16(0x24) & 0x01FF;
    v5 = abif_cpl_bl_read16(0x26);
    pOut->bwAdj = v5 & 0xFFF;
    v6 = abif_cpl_bl_read16(0x28);
    v7 = (v6 & 0x7FC0) >> 6;
    pOut->clkR = v6 & 0x3F;
    pOut->clkO0Div = v7;
    pOut->clkFMsb = abif_cpl_bl_read16(0x2A) & 0x0FFF;
    v9 = abif_cpl_bl_read16(0x2C);
    pOut->satEn = !!(v9 & 0x800);
    pOut->clkO1Div = v9 & 0x01FF;
    pOut->fastEn = !!(v9 & 0x1000);
    v13 = abif_cpl_bl_read16(0x2C);
    if ( (v13 & 0x4000) != 0 && (u16)(v13 & 0x8000) >> 15 == 1 )
    {
        pOut->operational = 1;
    }
    return 0;
}

int pll_usb_write(bsp_pll_cfg *pCfg)
{
    u16 v2; // r0
    u16 v3; // r4
    u16 v4; // r1
    u16 v5; // r3
    u16 v6; // r2
    u16 v7; // r1
    u16 v8; // r3
    u16 v9; // r2
    u16 v10; // r1
    u16 v11; // r3

    clear32(LT_CLOCKGATE_COMPAT, 0x360000);
    v2 = abif_cpl_bl_read16(0x2C);
    v3 = v2;
    if (v2 & 0x8000)
    {
        v3 = v2 & ~0x8000;
        abif_cpl_bl_write16(0x2C, v2 & ~0x8000);
        udelay(10);
    }

    abif_cpl_bl_write16(0x2C, v3 & ~0x4000);
    abif_cpl_bl_write16(0x24, pCfg->clkO2Div);
    abif_cpl_bl_write16(0x26, pCfg->bwAdj);
    abif_cpl_bl_write16(0x28,pCfg->clkR | 0x8000 | (u16)(pCfg->clkO0Div << 6));
    abif_cpl_bl_write16(0x2A, pCfg->clkFMsb);
    v4 = pCfg->satEn ? 0x800 : 0;
    v5 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_bl_write16(0x2C, (u16)v4 | (u16)(v5 | pCfg->clkO1Div));
    udelay(5);
    v7 = pCfg->satEn ? 0x4800 : 0x4000;
    v8 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_bl_write16(0x2C, (u16)v7 | (u16)(v8 | pCfg->clkO1Div));
    udelay(100);
    v10 = pCfg->satEn ? 0xC800 : 0xC000;
    v11 = pCfg->fastEn ? 0x1000 : 0;
    abif_cpl_bl_write16(0x2C, (u16)v10 | (u16)(v11 | pCfg->clkO1Div));
    udelay(100);

  set32(LT_CLOCKGATE_COMPAT, 0x360000);

  return 0;
}