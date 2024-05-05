/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "rtc.h"

#include "latte.h"
#include "exi.h"
#include "utils.h"

void rtc_set_ctrl1(u32 val)
{
    exi0_write32(0x21000D00, val);
}

u32 rtc_get_ctrl1()
{
    return exi0_read32(0x21000D00);
}

void rtc_set_ctrl0(u32 val)
{
    exi0_write32(0x21000C00, val);
}

u32 rtc_get_ctrl0()
{
    return exi0_read32(0x21000C00);
}

void rtc_get_panic_reason(char* buffer)
{
    u32* buf32 = (u32*)buffer;

    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, 0x20000100);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    for(int i = 0; i < 64 / sizeof(u32); i++)
    {
        write32(EXI0_CSR, 0x108);
        write32(EXI0_CR, 0x31);
        while(!(read32(EXI0_CSR) & 8));

        buf32[i] = read32(EXI0_DATA);
    }

    write32(EXI0_CSR, 0);
}

void rtc_set_panic_reason(const char* buffer)
{
    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, 0xA0000100);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    for(int i = 0; i < 64 / sizeof(u32); i++)
    {
        write32(EXI0_CSR, 0x108);
        write32(EXI0_DATA, read32_unaligned(&buffer[i * sizeof(u32)]));
        write32(EXI0_CR, 0x35);
        while(!(read32(EXI0_CSR) & 8));
    }

    write32(EXI0_CSR, 0);
}