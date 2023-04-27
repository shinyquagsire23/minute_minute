/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "console.h"

#include "gfx.h"
#include "serial.h"
#include <string.h>

char console[MAX_LINES][MAX_LINE_LENGTH];
int background_color = BLACK;
int text_color = WHITE;
int border_color = 0x3F7C7C;
int lines = 0;
int console_x = CONSOLE_X, console_y = CONSOLE_Y, console_w = CONSOLE_WIDTH, console_h = CONSOLE_HEIGHT;
int border_width = 3;

void console_init()
{
    console_flush();
}

void console_set_xy(int x, int y)
{
    console_x = x;
    console_y = y;
}

void console_get_xy(int *x, int *y)
{
    *x = console_x;
    *y = console_y;
}

void console_set_wh(int width, int height)
{
    console_w = width;
    console_h = height;
}

void console_set_border_width(int width)
{
    border_width = width;
}

int console_get_border_width(int width)
{
    return border_width;
}

void console_show()
{
    int i = 0, x = 0, y = 0;
    for(y = console_y; y < console_h + console_y + border_width; y++)
    {
        for(x = console_x; x < console_w + console_x + border_width; x++)
        {
            if((x >= console_x && x <= console_x + border_width) ||
               (y >= console_y && y <= console_y + border_width) ||
               (x >= console_w + console_x - 1 && x <= console_w + console_x - 1 + border_width) ||
               (y >= console_h + console_y - 1 && y <= console_h + console_y - 1 + border_width))
                gfx_draw_plot(GFX_DRC, x, y, border_color);
            else
                gfx_draw_plot(GFX_DRC, x, y, background_color);
        }
    }

    for(i = 0; i < lines; i++) {
        gfx_draw_string(GFX_DRC, console[i], console_x + CHAR_WIDTH * 2, i * CHAR_WIDTH + console_y + CHAR_WIDTH * 2, text_color);
        if (gfx_is_currently_headless()) {
            printf("%s\n", console[i]);
        }
    }
}

void console_flush()
{
    if (gfx_is_currently_headless())
    {
        serial_clear();
    }

    gfx_clear(GFX_DRC, BLACK);
    lines = 0;
}

void console_add_text(char* str)
{
    if(lines + 1 > MAX_LINES) console_flush();
    int i = 0, j = 0;
    for(i = 0; i < strlen(str); i++)
    {
        if(str[i] == '\n' || (str[i] == '\\' && str[i+1] == 'n') || j == MAX_LINE_LENGTH)
        {
            while((str[i] == '\\' && str[i+1] == 'n') || str[i] == '\n') i++;
            console[lines][j++] = 0;
            j = 0; lines++;
        }
        console[lines][j++] = str[i];
    }
    console[lines++][j] = 0;
}

void console_set_background_color(int color)
{
    background_color = color;
}

int console_get_background_color()
{
    return background_color;
}

void console_set_border_color(int color)
{
    border_color = color;
}

int console_get_border_color()
{
    return border_color;
}

void console_set_text_color(int color)
{
    text_color = color;
}

int console_get_text_color()
{
    return text_color;
}
