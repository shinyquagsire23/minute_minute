/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "latte.h"
#include "crypto.h"
#include "irq.h"
#include "utils.h"
#include "gfx.h"

extern seeprom_t seeprom;

int latte_get_wood_hw_version(u32 *pOut)
{
    u32 bsp_hw_version = BSP_HARDWARE_VERSION_UNKNOWN;
    u8 chiprev = read32(LT_WOOD_CHIPREVID) & 0xFF;
    int ret = 0;

    switch (chiprev)
    {
        case 0x0:
            bsp_hw_version = BSP_HARDWARE_VERSION_HOLLYWOOD_ENG_SAMPLE_1;
            break;
        case 0x10:
            bsp_hw_version = BSP_HARDWARE_VERSION_HOLLYWOOD_ENG_SAMPLE_2;
            break;
        case 0x11:
            bsp_hw_version = BSP_HARDWARE_VERSION_HOLLYWOOD_CORTADO;
            break;
        case 0x20:
            bsp_hw_version = BSP_HARDWARE_VERSION_BOLLYWOOD;
            break;
        case 0x21:
            bsp_hw_version = BSP_HARDWARE_VERSION_BOLLYWOOD_PROD_FOR_WII;
            break;
        default:
            bsp_hw_version = BSP_HARDWARE_VERSION_UNKNOWN;
            ret = 0x800;
            break;
    }

    *pOut = bsp_hw_version;
    return ret;
}

int latte_get_latte_hw_version(u32 *pOut)
{
    u32 bsp_hw_version = BSP_HARDWARE_VERSION_UNKNOWN;

    u32 chiprev = read32(LT_CHIPREVID);
    u8 version = chiprev & 0xFF;
    if ((chiprev & 0xFFFF0000) != 0xCAFE0000) {
        return 0x800;
    }
    
    u32 wood_version = *pOut & 0xFFFFF;
    *pOut = wood_version;

    switch (version)
    {
        case 0x10:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A11;
            break;
        case 0x18:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A12;
            break;
        case 0x21:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A2X;
            break;
        case 0x30:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A3X;
            break;
        case 0x40:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A4X;
            break;
        case 0x50:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_A5X;
            break;
        case 0x60:
        default:
            bsp_hw_version = BSP_HARDWARE_VERSION_LATTE_B1X;
            break;
    }

    *pOut = bsp_hw_version | wood_version;
    return 0;
}

u32 latte_get_hw_version()
{
    u32 version = BSP_HARDWARE_VERSION_UNKNOWN;

    int ret = latte_get_wood_hw_version(&version);
    if (ret) {
        return BSP_HARDWARE_VERSION_UNKNOWN;
    }

    if (version != BSP_HARDWARE_VERSION_BOLLYWOOD_PROD_FOR_WII || latte_get_latte_hw_version(&version))
    {
        return version;
    }

    int bspBoardType = 0;
    switch (seeprom.bc.board_type)
    {
        case 0x4556: // "EV"
            bspBoardType = BSP_HARDWARE_VERSION_EV;
            break;
        case 0x4944: // "ID"
            bspBoardType = BSP_HARDWARE_VERSION_ID;
            break;
        case 0x4948: // "IH"
            bspBoardType = BSP_HARDWARE_VERSION_IH;
            break;
        case 0x4346: // "CF"
            bspBoardType = BSP_HARDWARE_VERSION_CAFE;
            break;
        case 0x4354: // "CT"
            bspBoardType = BSP_HARDWARE_VERSION_CAT;
            break;
        default:
            bspBoardType = BSP_HARDWARE_VERSION_CAFE;//BSP_HARDWARE_VERSION_EV_Y;
            break;
    }

    version = (version & ~0xFF) | bspBoardType;

    return version;
}

void latte_set_iop_clock_mult(u8 val)
{
    u32 cookie = irq_kill();

   // Enable IRQ 12 (LT)
   write32(LT_INTSR_AHBLT_ARM, 0x1000);
   write32(LT_INTMR_AHBLT_ARM, 0x1000);

   // Switch the multiplier
   write32(LT_IOP2X, val & 0x3);

   // Wait for hardware interrupt
   irq_wait();

   // Disable IRQ 12 (LT)
   write32(LT_INTSR_AHBLT_ARM, 0);
   irq_restore(cookie);
}

void latte_print_hardware_info()
{
    const char* ddr_size_str = "unk";
    if (seeprom.bc.ddr3_size == 0x0800) {
        ddr_size_str = "2GiB";
    }
    else if (seeprom.bc.ddr3_size == 0x1000) {
        ddr_size_str = "3GiB";
    }
    printf("BSP version: 0x%08x\n", latte_get_hw_version());
    printf("Board type: %c%c (0x%02x)\n", seeprom.bc.board_type>>8, seeprom.bc.board_type&0xFF, seeprom.bc.board_type);
    printf("Board revision: 0x%x\n", seeprom.bc.board_revision);
    printf("DDR props: size=%s (0x%04x) speed=0x%04x vendor=%c%c (0x%04x)\n", ddr_size_str, seeprom.bc.ddr3_size, seeprom.bc.ddr3_speed, seeprom.bc.ddr3_vendor>>8, seeprom.bc.ddr3_vendor&0xFF, seeprom.bc.ddr3_vendor);
    printf("\n");
}