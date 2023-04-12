/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "exi.h"

#include "latte.h"
#include "utils.h"

void exi0_write32(u32 addr, u32 val)
{
    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, 0x80000000 | addr);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, val);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0);
}

u32 exi0_read32(u32 addr)
{
    u32 val = 0;

    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, addr & ~0x80000000);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0x108);
    write32(EXI0_CR, 0x31);
    while(!(read32(EXI0_CSR) & 8));

    val = read32(EXI0_DATA);

    write32(EXI0_CSR, 0);

    return val;
}