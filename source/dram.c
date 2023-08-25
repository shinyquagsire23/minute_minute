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
#include "pll.h"
#include "gpu.h"
#include <string.h>

// This entire file is *heavily* lifted from boot1/c2w, because frankly
// there's only one way to initialize DRAM correctly, and it's easier to
// diff regreads/regwrites in an emulator than to debug a custom-rolled
// impl. --Shiny

extern seeprom_t seeprom;

bsp_system_clock_info latte_clk_info = { 248625000, 1942382 };
u32 latte_hw_version = 0x25100028;
u32 dram_size_hi = 0;
u32 dram_size_lo = 0;
u32 bspHardwareVersion = 0;

int bsp_get_sys_clock_info(bsp_system_clock_info *pOut);
int dram_remove_memory_compat_mode(u16 mode);
void ddr_seq_write16(u16 seqAddr, u16 seqVal);

int to_pll_spll_write()
{
    return pll_spll_write(&spll_cfg);
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

int mem_clocks_related_3__3___DdrCafeInit(u16 mode)
{
    int v1; // lr
    unsigned int ddr_seq_tcl; // r7
    u16 ddr_seq_madj; // r5
    int v5; // r0
    u16 ddr_seq_sadj; // r4
    u16 v7; // r2
    char v8; // r3
    unsigned int v9; // r4
    u16 v10; // r1
    int v11; // r0
    int v15; // [sp+8h] [bp-B0h]
    u16 recen1; // [sp+12h] [bp-A6h]
    u16 recen0; // [sp+16h] [bp-A2h]
    int _sadj; // [sp+18h] [bp-A0h]
    unsigned int dram_mhz; // [sp+1Ch] [bp-9Ch]
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
        if ( (mode & DRAM_MODE_CCBOOT) || seeprom.bc.ddr3_speed == 1 )
        {
            memcpy(&pllCfg, &dram_3_pllcfg, sizeof(pllCfg));
            recen1 = 0;
            dram_mhz = 864;
            ddr_seq_madj = 0xFA;
            ddr_seq_tcl = 7;
            recen0 = 0x7C0;
            _sadj = 12;
            ddr_seq_twl = 6;
        }
        else if ( seeprom.bc.ddr3_speed == 3 )
        {
            memcpy(&pllCfg, &dram_2_pllcfg, sizeof(pllCfg));
            recen1 = 0;
            ddr_seq_tcl = 11;
            ddr_seq_madj = 0x82;
            recen0 = 0xFC00;
            _sadj = 11;
            dram_mhz = 1431;
            ddr_seq_twl = 8;
        }
        else // seeprom.bc.ddr3_speed == 2, 0, ...
        {
            memcpy(&pllCfg, &dram_1_pllcfg, sizeof(pllCfg));
            if ( (bspVer & 0xFFFF) == 16 )
            {
                recen1 = 1;
                ddr_seq_madj = 130;
                recen0 = 0xF800;
                _sadj = 11;
                dram_mhz = 1593;
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
                dram_mhz = 1593;
                ddr_seq_twl = 8;
            }
            ddr_seq_tcl = 11;
        }

        v15 = dram_remove_memory_compat_mode(mode);
        if ( v15 )
            goto LABEL_35;
        write16(MEM_COMPAT, 4);
        read16(MEM_COMPAT);
        if (mode & DRAM_MODE_20)
        {
            memset(&pllCfg2, 0, sizeof(pllCfg2));
            v5 = pll_dram_read(&pllCfg2);
            if ( v5 )
            {
LABEL_36:
                v15 = v5;
LABEL_35:
                write16(MEM_EDRAM_REFRESH_CTRL, 0xB);
                write16(MEM_EDRAM_REFRESH_VAL, 0xAFF);
                write16(MEM_EDRAM_REFRESH_CTRL, 0xE);
                write16(MEM_EDRAM_REFRESH_VAL, 0x1222);
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
            v5 = pll_dram_write(&pllCfg);
            if ( v5 )
                goto LABEL_36;
        }
        write16(MEM_COMPAT, 0); // c2w sets this to 4
        read16(MEM_COMPAT);
        ddr_seq_write16(DDR_SEQ_SYNC, 0);
        ddr_seq_write16(DDR_SEQ_RSTB, (mode & DRAM_MODE_SREFRESH) != 0);      // DDR_SREFRESH
        ddr_seq_write16(DDR_SEQ_CKEEN, 0);
        write16(MEM_REFRESH_FLAG, 0);
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
        read16(MEM_SEQ_REG_VAL);
        read16(MEM_SEQ0_REG_VAL);
        udelay(2);
        ddr_seq_write16(DDR_SEQ_SYNC, 0);
        write16(MEM_ARB_MISC, 0xEFF);
        if (!(mode & DRAM_MODE_CCBOOT))
        {
            // Idk1
            write16(MEM_REG_BASE + 0x600, 0x5555);
            write16(MEM_REG_BASE + 0x602, 21);
            write16(MEM_REG_BASE + 0x604, 15);
            write16(MEM_REG_BASE + 0x606, 0);
            write16(MEM_REG_BASE + 0x608, 0);
            write16(MEM_REG_BASE + 0x60A, 0);
            write16(MEM_REG_BASE + 0x60C, 0);
            write16(MEM_REG_BASE + 0x60E, 0);
            write16(MEM_REG_BASE + 0x610, 0);

            // Idk2
            write16(MEM_REG_BASE + 0x612, 0x5555);
            write16(MEM_REG_BASE + 0x614, 53);
            write16(MEM_REG_BASE + 0x616, 15);
            write16(MEM_REG_BASE + 0x618, 0);
            write16(MEM_REG_BASE + 0x61A, 0);
            write16(MEM_REG_BASE + 0x61C, 0);
            write16(MEM_REG_BASE + 0x61E, 0);
            write16(MEM_REG_BASE + 0x620, 0);
            write16(MEM_REG_BASE + 0x622, 0);

            // Idk3
            write16(MEM_REG_BASE + 0x624, 0x5555);
            write16(MEM_REG_BASE + 0x626, 53);
            write16(MEM_REG_BASE + 0x628, 15);
            write16(MEM_REG_BASE + 0x62A, 0);
            write16(MEM_REG_BASE + 0x62C, 0);
            write16(MEM_REG_BASE + 0x62E, 0);
            write16(MEM_REG_BASE + 0x630, 0);
            write16(MEM_REG_BASE + 0x632, 0);
            write16(MEM_REG_BASE + 0x634, 0);

            // Idk4
            write16(MEM_REG_BASE + 0x636, 0x5555);
            write16(MEM_REG_BASE + 0x638, 53);
            write16(MEM_REG_BASE + 0x63A, 15);
            write16(MEM_REG_BASE + 0x63C, 0);
            write16(MEM_REG_BASE + 0x63E, 0);
            write16(MEM_REG_BASE + 0x640, 0);
            write16(MEM_REG_BASE + 0x642, 0);
            write16(MEM_REG_BASE + 0x644, 0);
            write16(MEM_REG_BASE + 0x646, 0);

            // Idk1
            write16(MEM_REG_BASE + 0x4E6, 0x970);
            write16(MEM_REG_BASE + 0x4E8, 0);
            write16(MEM_REG_BASE + 0x4EA, 0);
            read16(MEM_REG_BASE + 0x4E8);
            read16(MEM_REG_BASE + 0x4EA);

            // Idk2
            write16(MEM_REG_BASE + 0x4E0, 0x83B);
            write16(MEM_REG_BASE + 0x4E2, 0x1FD);
            write16(MEM_REG_BASE + 0x4E4, 0);
            read16(MEM_REG_BASE + 0x4E8);
            read16(MEM_REG_BASE + 0x4EA);

            // Idk3
            write16(MEM_REG_BASE + 0x4E0, 0x809);
            write16(MEM_REG_BASE + 0x4E2, 0);
            write16(MEM_REG_BASE + 0x4E4, 0xFFFF);
            read16(MEM_REG_BASE + 0x4E8);
            read16(MEM_REG_BASE + 0x4EA);

            // Idk4
            write16(MEM_REG_BASE + 0x4E0, 0x835);
            write16(MEM_REG_BASE + 0x4E2, 0x8016);
            write16(MEM_REG_BASE + 0x4E4, 0);
            read16(MEM_REG_BASE + 0x4E8);
            read16(MEM_REG_BASE + 0x4EA);
        }
        write16(MEM_SEQRD_HWM, 8);
        write16(MEM_SEQWR_HWM, 12);
        write16(MEM_SEQCMD_HWM, 24);
        write16(MEM_ARB_MAXWR, 6);
        write16(MEM_ARB_MINRD, 6);
        write16(MEM_WRMUX, 8);
        write16(MEM_CPUAHM_WR_T, 4);
        write16(MEM_ACC_WR_T, 4);
        write16(MEM_DMAAHM0_WR_T, 4);
        write16(MEM_DMAAHM1_WR_T, 4);
        write16(MEM_PI_WR_T, 4);
        write16(MEM_PE_WR_T, 5);
        write16(MEM_IO_WR_T, 4);
        write16(MEM_DSP_WR_T, 4);
        write16(MEM_ACC_WR_T, 4);
        set16(MEM_UNK_306, 1u);
        write16(MEM_RDPR_PI, 16);
        write16(MEM_COLMSK, 0x3FF);
        write16(MEM_ROWMSK, 0x7FFF);
        write16(MEM_BANKMSK, 7);
        write16(MEM_RANKSEL, 5);
        write16(MEM_COLSEL, 4);
        write16(MEM_ROWSEL, 5);
        write16(MEM_BANKSEL, 3);
        if ( seeprom.bc.ddr3_size == 0x1000 )
        {
            ddr_seq_write16(DDR_SEQ_RANK2, 1);
            v7 = 0xD000; // 3GB
        }
        else
        {
            ddr_seq_write16(DDR_SEQ_RANK2, 0);
            v7 = 0x9000; // 2GB
        }
        write16(MEM_CAFE_DDR_RANGE_TOP, v7);
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
          v16 = 0x1D70; // used in c2w
        else
          v16 = 0x1D30;
        if ( ddr_seq_twl == 8 )
          v17 = 0x8018; // used in c2w
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
            write16(MEM_ARB_EXADDR, v17);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 35);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_ARB_EXCMD, 37);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_UNK_2D0, v17); // c2w omits these regs
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 35);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_UNK_2D2, 37);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_ARB_EXADDR, 0xC000);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 35);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_ARB_EXCMD, 37);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_UNK_2D0, 0xC000);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 35);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_UNK_2D2, 37);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_ARB_EXADDR, 0x4040);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 35);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_ARB_EXCMD, 37);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_UNK_2D0, 0x4040);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 35);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_UNK_2D2, 37);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_ARB_EXADDR, v16);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 35);
            write16(MEM_ARB_EXCMD, 34);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_ARB_EXCMD, 37);
            write16(MEM_ARB_EXCMD, 36);
            write16(MEM_UNK_2D0, v16);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 35);
            write16(MEM_UNK_2D2, 34);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_UNK_2D2, 37);
            write16(MEM_UNK_2D2, 36);
            write16(MEM_ARB_EXADDR, 0xFFFF);
            write16(MEM_ARB_EXCMD, 0x10);
            write16(MEM_ARB_EXCMD, 17);
            write16(MEM_ARB_EXCMD, 0x10);
            write16(MEM_UNK_2D0, 0xFFFF);
            write16(MEM_UNK_2D2, 0x10);
            write16(MEM_UNK_2D2, 17);
            write16(MEM_UNK_2D2, 0x10);
            udelay(2);
            write16(MEM_ARB_EXADDR, 0xFFFF);
            write16(MEM_ARB_EXCMD, 32);
            write16(MEM_ARB_EXCMD, 33);
            write16(MEM_ARB_EXCMD, 32);
            write16(MEM_UNK_2D0, 0xFFFF);
            write16(MEM_UNK_2D2, 32);
            write16(MEM_UNK_2D2, 33);
            write16(MEM_UNK_2D2, 32);
            write16(MEM_ARB_EXADDR, 0xFFFF);
            write16(MEM_ARB_EXCMD, 2);
            write16(MEM_ARB_EXCMD, 3);
            write16(MEM_ARB_EXCMD, 2);
            write16(MEM_UNK_2D0, 0xFFFF);
            write16(MEM_UNK_2D2, 2);
            write16(MEM_UNK_2D2, 3);
            write16(MEM_UNK_2D2, 2);
        }
        v11 = 2000000u / dram_mhz;
        write16(MEM_REFRESH_FLAG, ((7800000u / v11) >> 1) - 1);
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

void ddr_seq_write16(u16 seqAddr, u16 seqVal)
{
    write16(MEM_SEQ_REG_ADDR, seqAddr);
    write16(MEM_SEQ_REG_VAL, seqVal);
    write16(MEM_SEQ0_REG_ADDR, seqAddr);
    write16(MEM_SEQ0_REG_VAL, seqVal);
}

int mem_clocks_related_3__2___MCP_HWSetMEM2SelfRefreshMode(u16 mode)
{
    int v1; // lr
    int v2; // r4
    bsp_pll_cfg v4; // [sp+2h] [bp-52h] BYREF
    int v8; // [sp+54h] [bp+0h] BYREF

    memset(&v4, 0, sizeof(v4));
    v2 = pll_dram_read(&v4);
    if ( !v2 )
    {
        if ( v4.operational && (read32(LT_RESETS_COMPAT) & RSTB_MEM) != 0 )
        {
            write16(MEM_REFRESH_FLAG, 0);
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
        write16(MEM_EDRAM_REFRESH_CTRL, 0xB);
        write16(MEM_EDRAM_REFRESH_VAL, 0xAFF);
        write16(MEM_EDRAM_REFRESH_CTRL, 0xE);
        write16(MEM_EDRAM_REFRESH_VAL, 0x1222);
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

int mem_clocks_related_2(u16 mode)
{
    int ret = 0;

    ret = ret | mem_clocks_related_2__2();
    ret = ret | mem_clocks_related_2__3();
    if ( !(mode & DRAM_MODE_CCBOOT) )
        ret |= to_pll_spll_write();
    return pll_syspll_init(!!(mode & DRAM_MODE_CCBOOT)) | ret;
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
            ret = pll_syspll_read(0, &v7.systemClockFrequency);
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
        // This is "disable_2ndrank"
        dram_size_hi = 0;
        dram_size_lo = 0x80000000;
    }
    else if ( seeprom.bc.ddr3_size == 0x1000 ) // 3GiB
    {
        // This is the default in IOS?
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

// TODO does this belong here
void MCP_HWSetMEM1MapCompatMode()
{
  set32(MEM_MEM1_COMPAT_MODE, 3);
}