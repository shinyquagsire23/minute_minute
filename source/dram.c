/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "dram.h"
#include "latte.h"
#include "crypto.h"
#include "asic.h"
#include "utils.h"
#include "gpio.h"
#include <string.h>

// This entire file is *heavily* lifted from boot1/c2w, because frankly
// there's only one way to initialize DRAM correctly, and it's easier to
// diff regreads/regwrites in an emulator than to debug a custom-rolled
// impl. --Shiny

extern seeprom_t seeprom;

bsp_pll_cfg bspSysPllCfg248 =
{
    0, 1, 1, 0, 0, 0, 0, 0, 0x37, 0x1000, 0xC, 0x18, 0, 0x1C2, 0, 0xA, 5, 0
};

bsp_pll_cfg bspSysPllCfg240 =
{
    0, 1, 1, 0, 0, 0, 0, 0, 0x35, 0x1000, 0xC, 0x18, 0, 0x1C2, 0, 9, 5, 0
};

bsp_pll_cfg bspSysPllCfg243 =
{
    0, 1, 1, 0, 0, 0, 0, 0, 0x36, 0x0000, 0xC, 0x18, 0, 0x1C2, 0, 9, 5, 0
};

bsp_pll_cfg unkPllCfg1 = {0, 1, 1, 0, 1, 0, 1, 0, 0x3B, 0,      2, 0, 0, 0x1C2, 0, 0xA, 6, 0};
bsp_pll_cfg unkPllCfg2 = {0, 1, 1, 0, 1, 0, 1, 0, 0x35, 0,      2, 0, 0, 0x1C2, 0, 0xA, 5, 0};
bsp_pll_cfg unkPllCfg3 = {0, 1, 1, 0, 0, 0, 1, 0, 0x40, 0,      4, 0, 0, 0x1C2, 0, 0xB, 7, 0};
bsp_pll_cfg unkPllCfg4 = {0, 1, 1, 1, 0, 0, 0, 0, 0x28, 0x2F68, 4, 4, 0, 0x1C2, 0, 0x7, 4, 0};

bsp_system_clock_info latte_clk_info = { 248625000, 1942382 };
u32 latte_hw_version = 0x25100028;
u32 dram_size_hi = 0;
u32 dram_size_lo = 0;
u32 bspHardwareVersion = 0;

int bsp_get_sys_clock_info(bsp_system_clock_info *pOut);
int get_default_syspll_cfg(bsp_pll_cfg **ppCfg, u32 *pSysClkFreq);
int dram_remove_memory_compat_mode(u16 mode);
int init_br_pll_cfg_from_regs(bsp_pll_cfg *pOut);
void ddr_seq_write16(u16 seqAddr, u16 seqVal);

int gpu_related_1()
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

int abif_related_1(bsp_pll_cfg *pPllCfg)
{
    int v3; // r1
    int v4; // r2
    int v5; // r1
    int v6; // r2
    int v7; // r1
    int v8; // r0

    gpu_related_1();
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

int to_abif_related_1()
{
    return abif_related_1(&unkPllCfg4);
}

int mem_clocks_related_2__3()
{
    int v0; // lr
    unsigned int v1; // r7
    unsigned int v2; // r5
    unsigned int v3; // r12
    int v4; // r3
    int v6; // [sp+0h] [bp-2Ch]
    u32 bspVer; // [sp+4h] [bp-28h] BYREF
    int v11; // [sp+2Ch] [bp+0h] BYREF

    v6 = 0;
    bspVer = latte_get_hw_version();
    if (bspVer)
    {
        v1 = (read32(LT_IOSTRENGTH_CTRL0) & ~0x7000u | 0x2000) & ~0x38u | 8;
        v2 = (bspVer >> 25);
        u32 val_1E4 = read32(LT_IOSTRENGTH_CTRL1);
        v3 = val_1E4 & ~0x1000FC0u;
        if ( v2 && v2 != 16 )
        {
            if ( v2 == 32 )
            {
                v1 = (v1 & ~0x38000u | 0x10000) & ~0xE00u | 0x400;
                v3 = (val_1E4 & ~0x1000FF8u | 0x10) & ~7u | 2;
                write32(LT_ACRCLK_STRENGTH_CTRL, (((read32(LT_ACRCLK_STRENGTH_CTRL) & ~0xE00u | 0x800) & ~0x1C0u | 0x100) & ~0x38u | 0x20) & ~7u | 4);
            }
            else if (bspVer & 0xF000000)
            {
                if ((bspVer & 0xFFFF) == BSP_HARDWARE_VERSION_EV_Y)
                    v4 = 0xa0;
                else
                    v4 = 0x80;
                v1 = (v1 & ~0xE00u | (16 * v4)) & ~0x38000u | 0x20000;
                v3 = (val_1E4 & ~0x1000FF8u | 0x18) & ~7u | 3;
                write32(LT_IOSTRENGTH_CTRL2, (read32(LT_IOSTRENGTH_CTRL2) & ~7u | 3) & ~0x38u | 0x18);
                write32(LT_ACRCLK_STRENGTH_CTRL, (((read32(LT_ACRCLK_STRENGTH_CTRL) & ~0xE00u | 0x600) & ~0x1C0u | 0xC0) & ~0x38u | 0x18) & ~7u | 3);
            }
            else
            {
                v6 = 0x800;
            }
        }
        else
        {
            v1 = (v1 & ~0x38000u | 0x10000) & ~0xE00u | 0x400;
            v3 = (val_1E4 & ~0x1000FF8u | 0x20) & ~7u | 4;
        }
        write32(LT_IOSTRENGTH_CTRL0, v1);
        write32(LT_IOSTRENGTH_CTRL1, v3);
    }
    return v6;
}

int mem_clocks_related_2__2()
{
    int v0; // lr
    int v1; // r4
    int v2; // r1
    int v3; // r2
    int v4; // r3
    u32 bspVer; // [sp+0h] [bp-1Ch] BYREF
    int v10; // [sp+1Ch] [bp+0h] BYREF

    v1 = 0;
    bspVer = latte_get_hw_version();
    if ( bspVer )
    {
        if ( bspVer == 1 )
        {
            v2 = LT_BOOT0;
            write32(0xD8B0010, 0);
        }
        else
        {
            write32(0xD8B0010, 0);
            if ( bspVer != 0x10000001 )
            {
                set32(LT_BOOT0, 0x400u);
                set32(LT_BOOT0, 0x800u);
                if ( (bspVer & 0xF000000) == 0 || (bspVer >> 24) <= 0x22u )
                    goto LABEL_11;
                v2 = 0xD8B0854;
                v3 = read32(0xD8B0854);
                v4 = 516096;
LABEL_10:
                *(vu32*)v2 = v3 | v4;
LABEL_11:
                set32(LT_COMPAT_MEMCTRL_WORKAROUND, 0x1E);
                return v1;
            }
            set32(LT_ARB_CFG, 1);
            clear32(LT_ARB_CFG, 0xFFF0u);
            v2 = LT_BOOT0;
        }
        set32(LT_BOOT0, 0x400u);
        v3 = read32(LT_BOOT0);
        v4 = 0x800;
        goto LABEL_10;
    }
    return v1;
}

int ddrBrIdk(bsp_pll_cfg *pCfg)
{
    u16 v2; // r1
    u16 v3; // r3
    u16 v4; // r0
    u16 v5; // r1
    u16 v6; // r2
    u16 v7; // r0
    u16 v8; // r1
    u16 v9; // r2
    u16 v10; // r1
    u16 v11; // r3
    u16 v12; // r1
    u16 v13; // r2
    u16 v14; // r1
    u16 v15; // r2

    if (pCfg->satEn)
        v2 = 0x800;
    else
        v2 = 0;
    if (pCfg->fastEn)
        v3 = 0x1000;
    else
        v3 = 0;
    abif_cpl_br_write16(0x20u, v2 | v3);
    udelay(5000);
    abif_cpl_br_write16(CPL_CLK_V_LSB, pCfg->clkVLsb);
    v4 = pCfg->clkVMsb;
    if (pCfg->ssEn)
        v5 = 1024;
    else
        v5 = 0;
    if (pCfg->dithEn)
        v6 = 0x800;
    else
        v6 = 0;
    abif_cpl_br_write16(CPL_CLK_V_MSB, v5 | v4 | v6);
    abif_cpl_br_write16(0x14u, pCfg->clkS);
    abif_cpl_br_write16(0x16u, pCfg->bwAdj);
    abif_cpl_br_write16(0x18u, pCfg->clkFLsb);
    abif_cpl_br_write16(0x1Cu, (pCfg->clkO0Div << 6) | pCfg->clkR | 0x8000);
    v7 = pCfg->clkFMsb;
    if (pCfg->bypVco)
        v8 = 0x1000;
    else
        v8 = 0;
    if (pCfg->bypOut)
        v9 = 0x2000;
    else
        v9 = 0;
    abif_cpl_br_write16(0x1Eu, v8 | v7 | v9);
    if (pCfg->satEn)
        v10 = 0x800;
    else
        v10 = 0;
    if (pCfg->fastEn)
        v11 = 0x1000;
    else
        v11 = 0;
    abif_cpl_br_write16(0x20u, v10 | v11);
    udelay(5);
    if (pCfg->satEn)
        v12 = 0x800;
    else
        v12 = 0;
    if (pCfg->fastEn)
        v13 = 0x1000;
    else
        v13 = 0;
    abif_cpl_br_write16(0x20u, v12 | v13 | 0x4000);
    udelay(5000);
    if (pCfg->satEn)
        v14 = 0x800;
    else
        v14 = 0;
    if (pCfg->fastEn)
        v15 = 0x1000;
    else
        v15 = 0;
    abif_cpl_br_write16(0x20u, v14 | v15 | 0xC000);
    udelay(5000);
    return 0;
}

int mem_clocks_related_3__3___DdrCafeInit(u16 mode)
{
    int v1; // lr
    int v2; // r1
    unsigned int ddr_seq_tcl; // r7
    u16 ddr_seq_madj; // r5
    int v5; // r0
    u16 ddr_seq_sadj; // r4
    u16 v7; // r2
    char v8; // r3
    unsigned int v9; // r4
    u16 v10; // r1
    int v11; // r0
    int v14; // [sp+4h] [bp-B4h]
    int v15; // [sp+8h] [bp-B0h]
    u16 recen1; // [sp+12h] [bp-A6h]
    u16 recen0; // [sp+16h] [bp-A2h]
    int _sadj; // [sp+18h] [bp-A0h]
    unsigned int v19; // [sp+1Ch] [bp-9Ch]
    int ddr_seq_twl; // [sp+20h] [bp-98h]
    bsp_pll_cfg pllCfg2; // [sp+24h] [bp-94h] BYREF
    bsp_pll_cfg pllCfg; // [sp+5Ah] [bp-5Eh] BYREF
    int v26; // [sp+B8h] [bp+0h] BYREF
    u32 bspVer;
    u16 v16;
    u16 v17;

    v15 = 0;
    memset(&pllCfg, 0, sizeof(pllCfg));
    bspVer = latte_get_hw_version();
    if ( bspVer )
    {
        v2 = seeprom.bc.ddr3_speed;
        if ( (mode & DRAM_MODE_CCBOOT) || v2 == 1 )
        {
            memcpy(&pllCfg, &unkPllCfg3, sizeof(pllCfg));
            recen1 = 0;
            v19 = 864;
            ddr_seq_madj = 0xFA;
            ddr_seq_tcl = 7;
            recen0 = 0x7C0;
            _sadj = 12;
            ddr_seq_twl = 6;
        }
        else if ( v2 == 3 )
        {
            memcpy(&pllCfg, &unkPllCfg2, sizeof(pllCfg));
            recen1 = 0;
            ddr_seq_tcl = 0xB;
            ddr_seq_madj = 0x82;
            recen0 = 0xFC00;
            _sadj = 0xB;
            v19 = 0x597;
            ddr_seq_twl = 8;
        }
        else
        {
            memcpy(&pllCfg, &unkPllCfg1, sizeof(pllCfg));
            if ( (bspVer & 0xFFFF) == 16 )
            {
                recen1 = 1;
                ddr_seq_madj = 130;
                recen0 = 0xF800;
                _sadj = 11;
                v19 = 0x639;
                ddr_seq_twl = 8;
            }
            else
            {
                if ( (bspVer & 0xFFFFu) - 40 > 1 )
                {
                    recen1 = 0;
                    ddr_seq_madj = 130;
                    recen0 = 0xF800;
                    _sadj = 6;
                }
                else
                {
                    ddr_seq_madj = 130;
                    recen1 = 0;
                    recen0 = 0xF800;
                    _sadj = 8;
                }
                v19 = 0x639;
                ddr_seq_twl = 8;
            }
            ddr_seq_tcl = 11;
        }

        v14 = seeprom.bc.ddr3_size;
        v15 = dram_remove_memory_compat_mode(mode);
        if ( v15 )
            goto LABEL_35;
        write16(0xD8B4200, 4);
        read16(0xD8B4200);
        if (mode & DRAM_MODE_20)
        {
            memset(&pllCfg2, 0, sizeof(pllCfg2));
            v5 = init_br_pll_cfg_from_regs(&pllCfg2);
            if ( v5 )
            {
LABEL_36:
                v15 = v5;
LABEL_35:
                write16(0xD8B42CC, 0xB);
                write16(0xD8B42CE, 0xAFF);
                write16(0xD8B42CC, 0xE);
                write16(0xD8B42CE, 0x1222);
                return v15;
            }
            if ( !pllCfg2.operational )
            {
                v5 = 16;
                goto LABEL_36;
            }
        }
        else
        {
            v5 = ddrBrIdk(&pllCfg);
            if ( v5 )
                goto LABEL_36;
        }
        write16(0xD8B4200, 0);
        read16(0xD8B4200);
        ddr_seq_write16(DDR_SEQ_SYNC, 0);
        ddr_seq_write16(DDR_SEQ_RSTB, (mode & DRAM_MODE_SREFRESH) != 0);      // DDR_SREFRESH
        ddr_seq_write16(DDR_SEQ_CKEEN, 0);
        write16(0xD8B4226, 0);
        ddr_seq_write16(DDR_SEQ_MADJL, ddr_seq_madj | (ddr_seq_madj << 8));
        ddr_seq_write16(DDR_SEQ_MADJH, ddr_seq_madj | (ddr_seq_madj << 8));
        ddr_seq_sadj = (_sadj | (_sadj << 8)) & 0xFFFF;
        ddr_seq_write16(DDR_SEQ_SADJ0L, ddr_seq_sadj);
        ddr_seq_write16(DDR_SEQ_SADJ0H, ddr_seq_sadj);
        ddr_seq_write16(DDR_SEQ_SADJ1L, ddr_seq_sadj);
        ddr_seq_write16(DDR_SEQ_SADJ1H, ddr_seq_sadj);
        ddr_seq_write16(DDR_SEQ_PAD1, 0xE0B);
        udelay(2);
        ddr_seq_write16(DDR_SEQ_PAD1, 0x60B);
        udelay(2);
        ddr_seq_write16(DDR_SEQ_PAD1, 0x20B);
        ddr_seq_write16(DDR_SEQ_PAD0, 0x8040);
        read16(0xD8B42C4);
        read16(0xD8B4300);
        udelay(2);
        ddr_seq_write16(DDR_SEQ_SYNC, 0);
        write16(0xD8B42B6, 0xEFF);
        if (!(mode & DRAM_MODE_CCBOOT))
        {
            write16(0xD8B4600, 0x5555);
            write16(0xD8B4602, 21);
            write16(0xD8B4604, 15);
            write16(0xD8B4606, 0);
            write16(0xD8B4608, 0);
            write16(0xD8B460A, 0);
            write16(0xD8B460C, 0);
            write16(0xD8B460E, 0);
            write16(0xD8B4610, 0);
            write16(0xD8B4612, 0x5555);
            write16(0xD8B4614, 53);
            write16(0xD8B4616, 15);
            write16(0xD8B4618, 0);
            write16(0xD8B461A, 0);
            write16(0xD8B461C, 0);
            write16(0xD8B461E, 0);
            write16(0xD8B4620, 0);
            write16(0xD8B4622, 0);
            write16(0xD8B4624, 0x5555);
            write16(0xD8B4626, 53);
            write16(0xD8B4628, 15);
            write16(0xD8B462A, 0);
            write16(0xD8B462C, 0);
            write16(0xD8B462E, 0);
            write16(0xD8B4630, 0);
            write16(0xD8B4632, 0);
            write16(0xD8B4634, 0);
            write16(0xD8B4636, 0x5555);
            write16(0xD8B4638, 53);
            write16(0xD8B463A, 15);
            write16(0xD8B463C, 0);
            write16(0xD8B463E, 0);
            write16(0xD8B4640, 0);
            write16(0xD8B4642, 0);
            write16(0xD8B4644, 0);
            write16(0xD8B4646, 0);

            write16(0xD8B44E6, 0x970);
            write16(0xD8B44E8, 0);
            write16(0xD8B44EA, 0);
            read16(0xD8B44E8);
            read16(0xD8B44EA);

            write16(0xD8B44E0, 0x83B);
            write16(0xD8B44E2, 0x1FD);
            write16(0xD8B44E4, 0);
            read16(0xD8B44E8);
            read16(0xD8B44EA);

            write16(0xD8B44E0, 0x809);
            write16(0xD8B44E2, 0);
            write16(0xD8B44E4, 0xFFFF);
            read16(0xD8B44E8);
            read16(0xD8B44EA);

            write16(0xD8B44E0, 0x835);
            write16(0xD8B44E2, 0x8016);
            write16(0xD8B44E4, 0);
            read16(0xD8B44E8);
            read16(0xD8B44EA);
        }
        write16(0xD8B4268, 8);
        write16(0xD8B426A, 12);
        write16(0xD8B426C, 24);
        write16(0xD8B4280, 6);
        write16(0xD8B4282, 6);
        write16(0xD8B42BA, 8);
        write16(0xD8B426E, 4);
        write16(0xD8B4270, 4);
        write16(0xD8B4272, 4);
        write16(0xD8B4274, 4);
        write16(0xD8B4276, 4);
        write16(0xD8B4278, 5);
        write16(0xD8B427A, 4);
        write16(0xD8B427C, 4);
        write16(0xD8B427E, 4);
        set16(0xD8B4306, 1u);
        write16(0xD8B42A6, 16);
        write16(0xD8B4218, 0x3FF);
        write16(0xD8B421A, 0x7FFF);
        write16(0xD8B421C, 7);
        write16(0xD8B4216, 5);
        write16(0xD8B4210, 4);
        write16(0xD8B4212, 5);
        write16(0xD8B4214, 3);
        if ( v14 == 0x1000 )
        {
            ddr_seq_write16(DDR_SEQ_RANK2, 1);
            v7 = 0xD000;
        }
        else
        {
            ddr_seq_write16(DDR_SEQ_RANK2, 0);
            v7 = 0x9000;
        }
        write16(0xD8B42D6, v7);
        ddr_seq_write16(DDR_SEQ_TCL, ddr_seq_tcl - 1);
        ddr_seq_write16(DDR_SEQ_TWL, ddr_seq_twl - 2);
        ddr_seq_write16(DDR_SEQ_TRFC, 239);
        ddr_seq_write16(DDR_SEQ_TRCDW, 10);
        ddr_seq_write16(DDR_SEQ_TRCDR, 10);
        v8 = (2 * ddr_seq_twl) - 4;
        v9 = (unsigned int)(0x1FFC0 << v8);
        ddr_seq_write16(DDR_SEQ_QSOE0, v9);
        ddr_seq_write16(DDR_SEQ_QSOE1, v9 >> 16);
        ddr_seq_write16(DDR_SEQ_QSOE2, 0);
        ddr_seq_write16(DDR_SEQ_QSOE3, 0);
        ddr_seq_write16(DDR_SEQ_ODT0, 255);
        ddr_seq_write16(DDR_SEQ_ODT1, 0);
        if ( ddr_seq_tcl > 7 )
        {
            ddr_seq_write16(DDR_SEQ_RRL, 14);
            ddr_seq_write16(DDR_SEQ_NPLRD, 268);
            ddr_seq_write16(DDR_SEQ_TRDWR, ddr_seq_tcl - ddr_seq_twl + 9);
            ddr_seq_write16(DDR_SEQ_TWRRD, ddr_seq_twl + 9);
            ddr_seq_write16(DDR_SEQ_TRC, 38);
            v10 = ddr_seq_tcl + 4;
        }
        else
        {
            ddr_seq_write16(DDR_SEQ_RRL, 12);
            ddr_seq_write16(DDR_SEQ_NPLRD, 266);
            ddr_seq_write16(DDR_SEQ_TRDWR, ddr_seq_tcl - ddr_seq_twl + 9);
            ddr_seq_write16(DDR_SEQ_TWRRD, ddr_seq_twl + 9);
            ddr_seq_write16(DDR_SEQ_TRC, 38);
            v10 = 14;
        }
        ddr_seq_write16(DDR_SEQ_RDPR, v10);
        ddr_seq_write16(DDR_SEQ_WRPR, ddr_seq_twl + 26);
        ddr_seq_write16(DDR_SEQ_TRRD, 5);
        ddr_seq_write16(DDR_SEQ_TFAW, 32);
        ddr_seq_write16(DDR_SEQ_TR2R, 6);
        ddr_seq_write16(DDR_SEQ_RECEN0, recen0);
        ddr_seq_write16(DDR_SEQ_RECEN1, recen1);
        ddr_seq_write16(DDR_SEQ_IDLEST, 18);

        if ( ddr_seq_tcl == 11 )
          v16 = 0x1D70;
        else
          v16 = 0x1D30;
        if ( ddr_seq_twl == 8 )
          v17 = 0x8018;
        else
          v17 = 0x8008;

        ddr_seq_write16(DDR_SEQ_ODTDYN, 1);
        ddr_seq_write16(DDR_SEQ_ODTON, 0);
        ddr_seq_write16(DDR_SEQ_NPLCONF, 8);
        ddr_seq_write16(DDR_SEQ_BANK4, 0);
        ddr_seq_write16(DDR_SEQ_QSDEF, 1);
        ddr_seq_write16(DDR_SEQ_STR0, 0x318C);
        ddr_seq_write16(DDR_SEQ_STR1, 0x318C);
        ddr_seq_write16(DDR_SEQ_STR2, 0x318C);
        ddr_seq_write16(DDR_SEQ_STR3, 204);
        ddr_seq_write16(DDR_SEQ_APAD0, 0xE8EE);
        ddr_seq_write16(DDR_SEQ_APAD1, 3);
        ddr_seq_write16(DDR_SEQ_CKPAD0, 0xE8EE);
        ddr_seq_write16(DDR_SEQ_CKPAD1, 3);
        ddr_seq_write16(DDR_SEQ_CMDPAD0, 0xE8EE);
        ddr_seq_write16(DDR_SEQ_CMDPAD1, 3);
        ddr_seq_write16(DDR_SEQ_DQPAD0, 0xE6FE);
        ddr_seq_write16(DDR_SEQ_DQPAD1, 3);
        ddr_seq_write16(DDR_SEQ_QSPAD0, 0xF36E);
        ddr_seq_write16(DDR_SEQ_QSPAD1, 1);
        ddr_seq_write16(DDR_SEQ_BL4, 0);
        ddr_seq_write16(DDR_SEQ_DDR2, 1);
        ddr_seq_write16(DDR_SEQ_SYNC, 0);
        if (mode & DRAM_MODE_SREFRESH)
        {
            ddr_seq_write16(DDR_SEQ_CKEEN, 1);
            udelay(2);
        }
        else
        {
            udelay(200);
            ddr_seq_write16(DDR_SEQ_RSTB, 1);
            udelay(500);
            ddr_seq_write16(DDR_SEQ_CKEEN, 1);
        }
        if (!(mode & DRAM_MODE_10))
        {
            write16(0xD8B42C0, v17);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 35);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 36);
            write16(0xD8B42C2, 37);
            write16(0xD8B42C2, 36);
            write16(0xD8B42D0, v17);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 35);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 36);
            write16(0xD8B42D2, 37);
            write16(0xD8B42D2, 36);
            write16(0xD8B42C0, 0xC000);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 35);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 36);
            write16(0xD8B42C2, 37);
            write16(0xD8B42C2, 36);
            write16(0xD8B42D0, 0xC000);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 35);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 36);
            write16(0xD8B42D2, 37);
            write16(0xD8B42D2, 36);
            write16(0xD8B42C0, 0x4040);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 35);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 36);
            write16(0xD8B42C2, 37);
            write16(0xD8B42C2, 36);
            write16(0xD8B42D0, 0x4040);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 35);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 36);
            write16(0xD8B42D2, 37);
            write16(0xD8B42D2, 36);
            write16(0xD8B42C0, v16);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 35);
            write16(0xD8B42C2, 34);
            write16(0xD8B42C2, 36);
            write16(0xD8B42C2, 37);
            write16(0xD8B42C2, 36);
            write16(0xD8B42D0, v16);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 35);
            write16(0xD8B42D2, 34);
            write16(0xD8B42D2, 36);
            write16(0xD8B42D2, 37);
            write16(0xD8B42D2, 36);
            write16(0xD8B42C0, 0xFFFF);
            write16(0xD8B42C2, 0x10);
            write16(0xD8B42C2, 17);
            write16(0xD8B42C2, 0x10);
            write16(0xD8B42D0, 0xFFFF);
            write16(0xD8B42D2, 0x10);
            write16(0xD8B42D2, 17);
            write16(0xD8B42D2, 0x10);
            udelay(2);
            write16(0xD8B42C0, 0xFFFF);
            write16(0xD8B42C2, 32);
            write16(0xD8B42C2, 33);
            write16(0xD8B42C2, 32);
            write16(0xD8B42D0, 0xFFFF);
            write16(0xD8B42D2, 32);
            write16(0xD8B42D2, 33);
            write16(0xD8B42D2, 32);
            write16(0xD8B42C0, 0xFFFF);
            write16(0xD8B42C2, 2);
            write16(0xD8B42C2, 3);
            write16(0xD8B42C2, 2);
            write16(0xD8B42D0, 0xFFFF);
            write16(0xD8B42D2, 2);
            write16(0xD8B42D2, 3);
            write16(0xD8B42D2, 2);
        }
        v11 = 2000000u / v19;
        write16(0xD8B4226, ((7800000u / v11) >> 1) - 1);
        goto LABEL_35;
    }
    return v15;
}

int dram_remove_memory_compat_mode(u16 mode)
{
    u32 val_LT_RESETS_COMPAT = read32(LT_RESETS_COMPAT);
    u32 val_LT_COMPAT_MEMCTRL_STATE = read32(LT_COMPAT_MEMCTRL_STATE);

    if ( (mode & DRAM_MODE_40) == 0 )
    {
        val_LT_RESETS_COMPAT &= ~RSTB_MEM;
        write32(LT_RESETS_COMPAT, val_LT_RESETS_COMPAT);
        write32(LT_COMPAT_MEMCTRL_STATE, val_LT_COMPAT_MEMCTRL_STATE & ~0xC00);

        val_LT_RESETS_COMPAT |= RSTB_MEM;
        write32(LT_RESETS_COMPAT, val_LT_RESETS_COMPAT);
        read32(LT_RESETS_COMPAT);
        return 0;
    }
    if ( (val_LT_RESETS_COMPAT & RSTB_MEM) != 0 && (val_LT_COMPAT_MEMCTRL_STATE & 0x400) == 0 && (val_LT_COMPAT_MEMCTRL_STATE & 0x800) == 0 )
        return 0;
    return 16;
}

// This one had a ton of HIBYTEs
int init_br_pll_cfg_from_regs(bsp_pll_cfg *pOut)
{
    u16 v2; // r0
    int v8; // r0
    u16 v11; // r0
    int v12; // r2
    int v13; // r0
    u16 v14; // r0
    int v17; // r0

    memset(pOut, 0, sizeof(bsp_pll_cfg));
    pOut->clkVLsb = abif_cpl_br_read16(CPL_CLK_V_LSB);

    v2 = abif_cpl_br_read16(CPL_CLK_V_MSB);
    pOut->clkVMsb = (v2 & 0x3FFu);
    pOut->ssEn = !!(v2 & 0x400);
    pOut->dithEn = !!(v2 & 0x800);

    pOut->clkS = abif_cpl_br_read16(0x14u) & 0xFFF;
    pOut->bwAdj = abif_cpl_br_read16(0x16u) & 0xFFF;

    pOut->clkFLsb = abif_cpl_br_read16(0x18u) & 0x3FFF;
    v8 = abif_cpl_br_read16(0x1Cu);
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

void ddr_seq_write16(u16 seqAddr, u16 seqVal)
{
    write16(0xD8B42C6, seqAddr);
    write16(0xD8B42C4, seqVal);
    write16(0xD8B4302, seqAddr);
    write16(0xD8B4300, seqVal);
}

int mem_clocks_related_3__2___MCP_HWSetMEM2SelfRefreshMode(u16 mode)
{
    int v1; // lr
    int v2; // r4
    bsp_pll_cfg v4; // [sp+2h] [bp-52h] BYREF
    int v8; // [sp+54h] [bp+0h] BYREF

    memset(&v4, 0, sizeof(v4));
    v2 = init_br_pll_cfg_from_regs(&v4);
    if ( !v2 )
    {
        if ( v4.operational && (read32(LT_RESETS_COMPAT) & RSTB_MEM) != 0 )
        {
            write16(0xD8B4226, 0);
            ddr_seq_write16(DDR_SEQ_CKEEN, 1);
            ddr_seq_write16(DDR_SEQ_CKEDYN, 0);
            ddr_seq_write16(DDR_SEQ_CKESR, 1);
        }
        else
        {
            v2 = 0x10;
        }
    }
    return v2;
}

int mem_clocks_related_3__1__mcpEdramCafeEnableRefresh(u16 mode)
{
    int result; // r0

    result = dram_remove_memory_compat_mode(mode);
    if ( !result )
    {
        write16(0xD8B42CC, 0xB);
        write16(0xD8B42CE, 0xAFF);
        write16(0xD8B42CC, 0xE);
        write16(0xD8B42CE, 0x1222);
    }
    return result;
}

int mem_clocks_related_3(u16 mode)
{
    if (mode & DRAM_MODE_1)
        return mem_clocks_related_3__1__mcpEdramCafeEnableRefresh(mode);
    if (mode & DRAM_MODE_4)
        return mem_clocks_related_3__2___MCP_HWSetMEM2SelfRefreshMode(mode);
    return mem_clocks_related_3__3___DdrCafeInit(mode);
}

int bsp_init_sys_pll(bsp_pll_cfg *pParams)
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

int bsp_init_default_syspll(int bIdk)
{
    int result = 0;
    bsp_pll_cfg cfg;
    bsp_pll_cfg *ppCfg;

    ppCfg = 0;
    result = get_default_syspll_cfg(&ppCfg, 0);
    if ( !result )
    {
        memcpy(&cfg, ppCfg, sizeof(cfg));
        if ( bIdk )
            cfg.options |= 1;
        result = bsp_init_sys_pll(&cfg);
    }
    return result;
}

int mem_clocks_related_2(u16 mode)
{
    int ret = 0;

    ret = ret | mem_clocks_related_2__2();
    ret = ret | mem_clocks_related_2__3();
    if ( !(mode & DRAM_MODE_CCBOOT) )
        ret |= to_abif_related_1();
    return bsp_init_default_syspll(!!(mode & DRAM_MODE_CCBOOT)) | ret;
}

int mem2_get_clk_info()
{
    int v0; // r5
    int result; // r0

    latte_hw_version = latte_get_hw_version();
    if ( !latte_hw_version | bsp_get_sys_clock_info(&latte_clk_info) )
        result = -2;
    else
        result = 0;
    return result;
}

int get_default_syspll_cfg(bsp_pll_cfg **ppCfg, u32 *pSysClkFreq)
{
    bsp_pll_cfg *pCfg = NULL;
    u32 freq = 0;
    int result = 0;

    u32 bspVer = latte_get_hw_version();
    if (!bspVer)
        return -1;

    if ( (seeprom.bc.library_version) <= 2u )
    {
        pCfg = &bspSysPllCfg243;
        freq = 243000000;
    }
    else if ( seeprom.bc.sys_pll_speed == 0xF0 )
    {
        pCfg = &bspSysPllCfg240;
        freq = 239625000;
    }
    else if ( seeprom.bc.sys_pll_speed == 0xF8 )
    {
        pCfg = &bspSysPllCfg248;
        freq = 248625000;
    }
    else
    {
        pCfg = &bspSysPllCfg243;
        freq = 243000000;
    }

    if ( ppCfg )
        *ppCfg = pCfg;

    if ( pSysClkFreq )
        *pSysClkFreq = freq;
    return result;
}

int bsp_get_sys_clock_info(bsp_system_clock_info *pOut)
{
    int ret = 0;
    int v5; // r3
    bsp_system_clock_info v7; // [sp+0h] [bp-2Ch] BYREF
    int v12; // [sp+2Ch] [bp+0h] BYREF

    memset(&v7, 0, sizeof(v7));
    u32 bspVer = latte_get_hw_version();
    if ( (bspVer >> 24) && (bspVer >> 24) != 16 )
    {
        if (bspVer & 0x0F000000)
        {
            ret = get_default_syspll_cfg(0, &v7.systemClockFrequency);
            if (read32(LT_SYSPLL_CFG) & 1)
            {
                v7.systemClockFrequency >>= 1;
            }
        }
    }
    else
    {
        if (read32(LT_CLOCKINFO) & 2) {
            v7.systemClockFrequency = 162000000;
        }
        else {
            v7.systemClockFrequency = 243000000;
        }
        if ( bspVer == BSP_HARDWARE_VERSION_HOLLYWOOD_CORTADO || bspVer == BSP_HARDWARE_VERSION_HOLLYWOOD_CORTADO_ESPRESSO )
        {
            v7.systemClockFrequency = 167000000;
        }
    }
    v7.timerFrequency = (unsigned int)v7.systemClockFrequency >> 7;
    memcpy(pOut, &v7, sizeof(bsp_system_clock_info));
    return ret;
}

int init_mem2(u16 mem_mode)
{
    int ret = 0;

    mem2_get_clk_info();

    if ( !(seeprom.bc_size) )
    {
        dram_size_hi = 0;
        dram_size_lo = 0;
        ret = 0x40000;
    }
    else if ( seeprom.bc.ddr3_size == 0x800 ) // 2GiB
    {
        dram_size_hi = 0;
        dram_size_lo = 0x80000000;
    }
    else if ( seeprom.bc.ddr3_size == 0x1000 ) // 3GiB
    {
        dram_size_hi = 0;
        dram_size_lo = 0xC0000000;
    }
    else {
        ret = 0x40000;
    }

LABEL_8:
    ret = ret | mem_clocks_related_2(mem_mode);
    ret = ret | mem_clocks_related_3(mem_mode);
    return ret;
}
