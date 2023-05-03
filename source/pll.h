/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __PLL_H__
#define __PLL_H__

#include "types.h"

typedef struct bsp_pll_cfg
{
    u32 fastEn;
    u32 dithEn;
    u32 satEn;
    u32 ssEn;
    u32 bypOut;
    u32 bypVco;
    u32 operational;
    u16 clkR;
    u16 clkFMsb;
    u16 clkFLsb;
    u16 clkO0Div;
    u16 clkO1Div;
    u16 clkO2Div;
    u16 clkS;
    u16 clkVMsb;
    u16 clkVLsb;
    u32 bwAdj;
    u32 options;
} bsp_pll_cfg;

u64 pll_calc_frequency(bsp_pll_cfg* pCfg);

int pll_vi1_shutdown();
int pll_vi1_shutdown_alt();
int pll_vi2_shutdown();
int pll_vi1_write(bsp_pll_cfg *pCfg);
int pll_vi2_write(bsp_pll_cfg *pCfg);

int pll_upll_write(bsp_pll_cfg *pCfg);
int pll_upll_read(bsp_pll_cfg *pOut);
void pll_upll_init();

int pll_syspll_read(bsp_pll_cfg **ppCfg, u32 *pSysClkFreq);
int pll_syspll_write(bsp_pll_cfg *pParams);
int pll_syspll_init(int bIdk);

int pll_spll_write(bsp_pll_cfg *pPllCfg);

int pll_dram_read(bsp_pll_cfg *pOut);
int pll_dram_write(bsp_pll_cfg *pCfg);

int pll_usb_read(bsp_pll_cfg *pOut);
int pll_usb_write(bsp_pll_cfg *pCfg);

extern bsp_pll_cfg dram_1_pllcfg;
extern bsp_pll_cfg dram_2_pllcfg;
extern bsp_pll_cfg dram_3_pllcfg;
extern bsp_pll_cfg spll_cfg;

extern bsp_pll_cfg vi1_pllcfg;
extern bsp_pll_cfg vi2_pllcfg;

extern bsp_pll_cfg usbpll_cfg;

extern bsp_pll_cfg spll_cfg_customclock;
extern bsp_pll_cfg spll_cfg_overclock;
extern bsp_pll_cfg spll_cfg_underclock;

#endif // __PLL_H__