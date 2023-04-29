/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "serial.h"

#include "gpio.h"
#include "utils.h"
#include "gfx.h"
#include <string.h>

u8 serial_buffer[256];
u16 serial_len = 0;
static u8 _serial_allow_zeros = 0;
u32 serial_line = 0;

void serial_fatal()
{
    while (1) {
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

        serial_send(0xF0);
        serial_send(0x0F);
        serial_send(0xAA);
        serial_send(0xAA);
    }
}

void serial_force_terminate()
{
    gpio_debug_serial_send(0x0F);
    udelay(SERIAL_DELAY);

    gpio_debug_serial_send(0x8F);
    udelay(SERIAL_DELAY);

    gpio_debug_serial_send(0x0F);
    gpio_debug_serial_send(0x00);
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

int serial_in_read(u8* out) {
    memset(out, 0, sizeof(serial_buffer));
    memcpy(out, serial_buffer, serial_len);
    out[255] = 0;

    u16 read_len = serial_len;
    serial_len = 0;

    return read_len;
}

void serial_line_inc()
{
    serial_line++;
}

// Stuff like menu doesn't need to preserve history
void serial_line_noscroll()
{
    serial_line = 0;
}

void serial_clear()
{
    static int saved = 0;
    int get_serial_line = serial_line;

    if (!saved) {
        for (int i = 0; i < 256; i++)
        {
            serial_send('\n');
        }
        // Save the cursor position at the bottom of the screen
        serial_printf("\033[s");
        saved = 1;
    }

    // Restore cursor to last line, then push everything up
    serial_printf("\033[u");
    for (int i = 0; i < get_serial_line; i++)
    {
        serial_printf("\n"); // \033[2K
    }

    // Move cursor to home and clear screen
    serial_printf("\033[H\033[2J");
    serial_line = 0;
}

void serial_poll()
{
    serial_send(0);
}

void serial_allow_zeros()
{
    _serial_allow_zeros = 1;
}

void serial_disallow_zeros()
{
    _serial_allow_zeros = 0;
}

void serial_send(u8 val)
{
    u8 read_val = 0;
    u8 read_val_valid = 0;
    for (int j = 7; j >= 0; j--)
    {
        u8 bit = (val & (1<<j)) ? 1 : 0;
        gpio_debug_serial_send(bit);
        udelay(SERIAL_DELAY);
        if (j == 7) {
            read_val_valid = gpio_debug_serial_read();
        }
        gpio_debug_serial_send(0x80 | bit);
        udelay(SERIAL_DELAY);
        read_val <<= 1;
        read_val |= gpio_debug_serial_read();
        //gpio_debug_serial_send(0x0 | bit);
        //udelay(SERIAL_DELAY);
    }

    if (((read_val || _serial_allow_zeros) && read_val_valid) && serial_len < sizeof(serial_buffer)-1) {
        serial_buffer[serial_len++] = read_val;
    }

    serial_force_terminate();
}