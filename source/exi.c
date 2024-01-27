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

void exi_init(void)
{
    write32(EXI0_CSR, 0x280A);
    write32(EXI1_CSR, 0x80A);
    write32(EXI2_CSR, 0x80A);
}

void exi0_select(int device, int freq)
{
    mask32(EXI0_CSR, ~0x405, (0x80 << device) | (freq << 4) | 8);
}

void exi0_deselect(void)
{
    mask32(EXI0_CSR, ~0x405, 0);
}

void exi0_wait_complete(void)
{
    while(!(read32(EXI0_CSR) & 8));
}

// TODO device num
void exi0_write32(u32 addr, u32 val)
{
    exi0_select(1, 0);
    write32(EXI0_DATA, 0x80000000 | addr);
    write32(EXI0_CR, EXI_TRANSFER_LENGTH(4) | EXI_TRANSFER_TYPE_W | EXI_START_TRANSFER);
    exi0_wait_complete();

    exi0_select(1, 0);
    write32(EXI0_DATA, val);
    write32(EXI0_CR, EXI_TRANSFER_LENGTH(4) | EXI_TRANSFER_TYPE_W | EXI_START_TRANSFER);
    exi0_wait_complete();

    exi0_deselect();
}

// TODO device num
u32 exi0_read32(u32 addr)
{
    u32 val = 0;

    exi0_select(1, 0);
    write32(EXI0_DATA, addr & ~0x80000000);
    write32(EXI0_CR, EXI_TRANSFER_LENGTH(4) | EXI_TRANSFER_TYPE_W | EXI_START_TRANSFER);
    exi0_wait_complete();

    exi0_select(1, 0);
    write32(EXI0_CR, EXI_TRANSFER_LENGTH(4) | EXI_TRANSFER_TYPE_R | EXI_START_TRANSFER);
    exi0_wait_complete();

    val = read32(EXI0_DATA); // 0x0D806810

    exi0_deselect();

    return val;
}

void exi1_write32(u32 addr, u32 val)
{
#if 0
    write32(EXI0_CSR, 0x88);
    write32(EXI0_DATA, 0x80000000 | addr);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0x88);
    write32(EXI0_DATA, val);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0);

    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, 0x80000000 | addr);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0x108);
    write32(EXI0_DATA, val);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0);

    write32(EXI0_CSR, 0x208);
    write32(EXI0_DATA, 0x80000000 | addr);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0x208);
    write32(EXI0_DATA, val);
    write32(EXI0_CR, 0x35);
    while(!(read32(EXI0_CSR) & 8));

    write32(EXI0_CSR, 0);



    write32(EXI1_CSR, 0x88);
    write32(EXI1_DATA, 0x80000000 | addr);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0x88);
    write32(EXI1_DATA, val);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0);

    write32(EXI1_CSR, 0x108);
    write32(EXI1_DATA, 0x80000000 | addr);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0x108);
    write32(EXI1_DATA, val);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0);

    write32(EXI1_CSR, 0x208);
    write32(EXI1_DATA, 0x80000000 | addr);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0x208);
    write32(EXI1_DATA, val);
    write32(EXI1_CR, 0x35);
    while(!(read32(EXI1_CSR) & 8));

    write32(EXI1_CSR, 0);





    write32(EXI2_CSR, 0x88);
    write32(EXI2_DATA, 0x80000000 | addr);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0x88);
    write32(EXI2_DATA, val);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0);

    write32(EXI2_CSR, 0x108);
    write32(EXI2_DATA, 0x80000000 | addr);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0x108);
    write32(EXI2_DATA, val);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0);

    write32(EXI2_CSR, 0x208);
    write32(EXI2_DATA, 0x80000000 | addr);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0x208);
    write32(EXI2_DATA, val);
    write32(EXI2_CR, 0x35);
    while(!(read32(EXI2_CSR) & 8));

    write32(EXI2_CSR, 0);
#endif
}