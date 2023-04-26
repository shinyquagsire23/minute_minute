/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include "utils.h"
#include "latte.h"
#include "gpio.h"
#include "gfx.h"

#define eeprom_delay() udelay(5)

static void send_bits(u32 b, int bits)
{
    while(bits--)
    {
        if(b & (1 << bits))
            set32(LT_GPIO_OUT, BIT(GP_EEP_MOSI));
        else
            clear32(LT_GPIO_OUT, BIT(GP_EEP_MOSI));
        eeprom_delay();
        set32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
        eeprom_delay();
        clear32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
        eeprom_delay();
    }
}

static int recv_bits(int bits)
{
    int res = 0;
    while(bits--)
    {
        res <<= 1;
        set32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
        eeprom_delay();
        clear32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
        eeprom_delay();
        res |= !!(read32(LT_GPIO_IN) & BIT(GP_EEP_MISO));
    }
    return res;
}

// Make the SEEPROM latch the CS bit I guess?
static void flush()
{
    clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    recv_bits(2);
}

int seeprom_read(void *dst, int offset, int size)
{
    int i;
    u16 *ptr = (u16 *)dst;
    u16 recv;

    if(size & 1)
        return -1;

    gpio_set_dir(GP_EEP_CLK, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_CS, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_MOSI, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_MISO, GPIO_DIR_IN);

    gpio_enable(GP_EEP_CLK, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_CS, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_MOSI, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_MISO, GPIO_DIR_IN);

    clear32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
    clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    eeprom_delay();

    for(i = 0; i < size; ++i)
    {
        //printf("%x\n", i);
        set32(LT_GPIO_OUT, BIT(GP_EEP_CS));
        send_bits((0x600 | (offset + i)), 11);
        recv = recv_bits(16);
        *ptr++ = recv;
        clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
        flush();
        eeprom_delay();
    }

    return size;
}

int seeprom_write(void *src, int offset, int size)
{
    int i;
    u16 *ptr = (u16 *)src;
    u16 recv;

    if(size & 1)
        return -1;

    gpio_set_dir(GP_EEP_CLK, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_CS, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_MOSI, GPIO_DIR_OUT);
    gpio_set_dir(GP_EEP_MISO, GPIO_DIR_IN);

    gpio_enable(GP_EEP_CLK, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_CS, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_MOSI, GPIO_DIR_OUT);
    gpio_enable(GP_EEP_MISO, GPIO_DIR_IN);

    clear32(LT_GPIO_OUT, BIT(GP_EEP_CLK));
    clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    eeprom_delay();

    // Write enable
    set32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    send_bits(0x4C0, 11);
    clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    flush();
    eeprom_delay();

    for(i = 0; i < size; ++i)
    {
        //printf("%x\n", i);
        set32(LT_GPIO_OUT, BIT(GP_EEP_CS));
        send_bits(((0x500 | (offset + i)) << 16) | (*ptr++), 23);
        clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
        flush();
        eeprom_delay();
    }

    // Write disable
    set32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    send_bits(0x400, 11);
    clear32(LT_GPIO_OUT, BIT(GP_EEP_CS));
    flush();
    eeprom_delay();

    return size;
}

