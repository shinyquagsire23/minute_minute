/*
    mini - a Free Software replacement for the Nintendo/BroadOn IOS.

    ELF loader

Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#include "types.h"
#include "utils.h"
#include "start.h"
#include "hollywood.h"
#include "string.h"
#include "elf.h"

typedef struct {
    u32 hdrsize;
    u32 loadersize;
    u32 elfsize;
    u32 argument;
} ioshdr;

void serial_send(u8 val);

#define SERIAL_DELAY (1)

void gpio_debug_send(u8 val)
{
    *(vu32*)0xD8000DC |= 0x00FF0000; // Enable
    *(vu32*)0xD8000E4 |= 0x00FF0000; // Direction = Out

    *(vu32*)0xD8000E0 = (*(vu32*)0xD8000E0 & 0xFF00FFFF) | (val << 16);
    udelay(1);
}

void serial_force_terminate()
{
    gpio_debug_send(0x0F);
    udelay(SERIAL_DELAY);

    gpio_debug_send(0x8F);
    udelay(SERIAL_DELAY * 2);

    gpio_debug_send(0x0F);
    udelay(SERIAL_DELAY);

    gpio_debug_send(0x00);
    udelay(SERIAL_DELAY);
}

void serial_send_u32(u32 val)
{
    u8 b[4];
    *(u32*)b = val;

    for (int i = 0; i < 4; i++)
    {
        serial_send(b[i]);
    }
}

void serial_send(u8 val)
{
    for (int j = 7; j >= 0; j--)
    {
        u8 bit = (val & (1<<j)) ? 1 : 0;
        gpio_debug_send(0);
        udelay(SERIAL_DELAY);
        gpio_debug_send(bit);
        udelay(SERIAL_DELAY);
        gpio_debug_send(0x80 | bit);
        udelay(SERIAL_DELAY);
        gpio_debug_send(0x0 | bit);
        udelay(SERIAL_DELAY);
    }

    u8 bit = (val & 1) ? 1 : 0;
    gpio_debug_send(0x80 | bit);
    udelay(SERIAL_DELAY);
    gpio_debug_send(0x0 | bit);
    udelay(SERIAL_DELAY);

    serial_force_terminate();
}

#if 0
void serial_force_terminate()
{
    gpio_debug_send(0x0F);
    udelay(SERIAL_DELAY);

    gpio_debug_send(0x8F);
    udelay(SERIAL_DELAY * 2);

    gpio_debug_send(0x0F);
    udelay(SERIAL_DELAY);

    gpio_debug_send(0x00);
    udelay(SERIAL_DELAY);
}

void serial_send(u8 val)
{
    for (int j = 7; j >= 0; j--)
    {
        u8 bit = (val & (1<<j)) ? 1 : 0;
        gpio_debug_send(0);
        udelay(SERIAL_DELAY);
        gpio_debug_send(bit);
        udelay(SERIAL_DELAY);
        gpio_debug_send(0x80 | bit);
        udelay(SERIAL_DELAY);
        gpio_debug_send(0x0 | bit);
        udelay(SERIAL_DELAY);
    }

    u8 bit = (val & 1) ? 1 : 0;
    gpio_debug_send(0x80 | bit);
    udelay(SERIAL_DELAY);
    gpio_debug_send(0x0 | bit);
    udelay(SERIAL_DELAY);

    serial_force_terminate();
}
#endif

void *loadelf(const u8 *elf) {
    if(memcmp("\x7F" "ELF\x01\x02\x01",elf,7)) {
        panic(0xE3);
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr*)elf;
    if(ehdr->e_phoff == 0) {
        panic(0xE4);
    }
    int count = ehdr->e_phnum;
    Elf32_Phdr *phdr = (Elf32_Phdr*)(elf + ehdr->e_phoff);
    while(count--)
    {
        if(phdr->p_type == PT_LOAD) {
            const void *src = elf + phdr->p_offset;
            memcpy(phdr->p_paddr, src, phdr->p_filesz);
        }
        phdr++;
    }
    return ehdr->e_entry;
}

static inline void disable_boot0()
{
    set32(HW_BOOT0, 0x1000);
}

static inline void mem_setswap()
{
    set32(HW_MEMMIRR, 0x20);
}

u8 otp[0x400];

void crypto_read_otp(void)
{
    u32 *otpd = (u32*)&otp[0];
    int word, bank;
    for (bank = 0; bank < 8; bank++)
    {
        for (word = 0; word < 0x20; word++)
        {
            write32(LT_OTPCMD, 0x80000000 | bank << 8 | word);
            *otpd++ = read32(LT_OTPDATA);
        }
    }
}


void *_main(void *base)
{
    ioshdr *hdr = (ioshdr*)base;
    u8 *elf;
    void *entry;

    // boot1 doesn't have an IOS header
    int is_boot1 = 0;
    if (hdr->hdrsize > 0x1000) {
        hdr->hdrsize = 0x10;
        is_boot1 = 1;
    }
    else {
        mem_setswap(1);
    }

    if (is_boot1) {
        gpio_debug_send(0x88);
        udelay(10000);
    }
    

    /*while (1) {
        debug_send(0x88);
        udelay(250000);
        debug_send(0x0);
        udelay(250000);
    }*/

#if 0
    crypto_read_otp();

    while (1) {
        //gpio_debug_send(0xFF);
        //udelay(500000);

        serial_force_terminate();

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        for (int i = 0; i < 0x10; i++)
        {
            serial_send(otp[i]);
        }

        for (int i = 0; i < 0x20; i++)
        {
            serial_send(*(vu8*)(0x0D406000+i));
            *(vu8*)(0x0D406000+i) = 0xFF;
        }

        while (1) {
            udelay(1);
        }
    }
#endif

    elf = (u8*) base;
    elf += hdr->hdrsize + hdr->loadersize;

    disable_boot0(1);

    if (is_boot1) {
        gpio_debug_send(0x89);
    }

    if (!is_boot1) {
        serial_send_u32(0xF00FCAFF);
    }

    entry = loadelf(elf);
    if (is_boot1)
        gpio_debug_send(0x8A);
    if (!is_boot1) {
        serial_send_u32(0xF00FCAFA);
    }
    return entry;
}

