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
#include "smc.h"
#include <string.h>

char console[MAX_LINES][MAX_LINE_LENGTH];
int background_color = BLACK;
int text_color = WHITE;
int border_color = 0x3F7C7C;
int lines = 0;
int console_x = CONSOLE_X, console_y = CONSOLE_Y, console_w = CONSOLE_WIDTH, console_h = CONSOLE_HEIGHT;
int border_width = 3;
int console_tv_x = CONSOLE_TV_X, console_tv_y = CONSOLE_TV_Y, console_tv_w = CONSOLE_TV_WIDTH, console_tv_h = CONSOLE_TV_HEIGHT;


void console_init()
{
    console_flush();
    gfx_clear(GFX_TV, BLACK);
    gfx_clear(GFX_DRC, BLACK);
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
               (y >= console_h + console_y - 1 && y <= console_h + console_y - 1 + border_width)) {
                gfx_draw_plot(GFX_DRC, x, y, border_color);
            }
            else {
                //gfx_draw_plot(GFX_DRC, x, y, background_color);
            }
        }
    }

    for(y = console_tv_y; y < console_tv_h + console_tv_y + border_width; y++)
    {
        for(x = console_tv_x; x < console_tv_w + console_tv_x + border_width; x++)
        {
            if((x >= console_tv_x && x <= console_tv_x + border_width) ||
               (y >= console_tv_y && y <= console_tv_y + border_width) ||
               (x >= console_tv_w + console_tv_x - 1 && x <= console_tv_w + console_tv_x - 1 + border_width) ||
               (y >= console_tv_h + console_tv_y - 1 && y <= console_tv_h + console_tv_y - 1 + border_width)) {
                gfx_draw_plot(GFX_TV, x, y, border_color);
            }
            else {
                //gfx_draw_plot(GFX_DRC, x, y, background_color);
            }
        }
    }

    for(i = 0; i < lines; i++) {
        for (int j = console_x; j < console_w-CHAR_WIDTH; j += CHAR_WIDTH) {
            gfx_draw_string(GFX_DRC, " ", j + CHAR_WIDTH * 1, i * CHAR_WIDTH + console_y + CHAR_WIDTH * 2, background_color);
            gfx_draw_string(GFX_TV, " ", j + CHAR_WIDTH * 1, i * CHAR_WIDTH + console_y + CHAR_WIDTH * 2, background_color);
        }
        gfx_draw_string(GFX_DRC, console[i], console_x + CHAR_WIDTH * 1, i * CHAR_WIDTH + console_y + CHAR_WIDTH * 2, text_color);
        gfx_draw_string(GFX_TV, console[i], console_x + CHAR_WIDTH * 1, i * CHAR_WIDTH + console_y + CHAR_WIDTH * 2, text_color);
        //if (gfx_is_currently_headless()) 
        {
            serial_printf("%s\n", console[i]);
        }
    }
}

void console_flush()
{
    //if (gfx_is_currently_headless())
    {
        serial_clear();
    }

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

void console_wait_power_or_q()
{
    console_select_flush();
    while (1)
    {
        int input = console_select_poll();
        if ((input & CONSOLE_KEY_POWER) || (input & CONSOLE_KEY_Q)) return;
    }
}

void console_wait_power_q_eject_or_p()
{
    console_select_flush();
    while (1)
    {
        int input = console_select_poll();
        if ((input & CONSOLE_KEY_POWER) || (input & CONSOLE_KEY_Q)) return;
        if ((input & CONSOLE_KEY_EJECT) || (input & CONSOLE_KEY_P)) return;
    }
}

void console_power_or_eject_to_return()
{
    console_select_flush(); // Eat all existing events
    printf("Press POWER/Q or EJECT/P to return...\n");
    console_wait_power_q_eject_or_p();
}

void console_power_to_exit()
{
    console_select_flush(); // Eat all existing events
    printf("Press POWER/Q to exit.\n");
    console_wait_power_or_q();
}

void console_power_to_continue()
{
    console_select_flush(); // Eat all existing events
    printf("Press POWER/Q to continue.\n");
    console_wait_power_or_q();
}

int console_abort_confirmation(const char* text_power, const char* text_eject)
{
    printf("[POWER/Q] %s | [EJECT/P] %s...\n", text_power, text_eject);

    console_select_flush();

    while (1)
    {
        int input = console_select_poll();
        if ((input & CONSOLE_KEY_POWER) || (input & CONSOLE_KEY_Q)) return 1;
        if ((input & CONSOLE_KEY_EJECT) || (input & CONSOLE_KEY_P)) return 0;
    }

    return 0;
}

int console_abort_confirmation_power_no_eject_yes()
{
    return console_abort_confirmation("No", "Yes");
}

int console_abort_confirmation_power_exit_eject_continue()
{
    return console_abort_confirmation("Exit", "Continue");
}

int console_abort_confirmation_power_skip_eject_dump()
{
    return console_abort_confirmation("Skip", "Dump");
}

static char console_serial_tmp[256];
static int console_serial_len = 0;
static int parsing_escape_code = 0;
static int parsing_csi = 0;

int console_konami_code()
{
    int step = 0;
    printf("Please enter the Konami code to continue.\n");
    printf("[POWER/Q] Abort\n");
    printf("[^ ^ v v < > < > B A ENTER] Continue...\n");

    console_select_flush();

    while (true)
    {
        int key = console_select_poll();
        if (!key) continue;

        if ((key & CONSOLE_KEY_POWER) || (key & CONSOLE_KEY_Q)) return 1;

        if ((step == 0 || step == 1) && key == CONSOLE_KEY_UP) {
            printf("^ ");
            step++;
            continue;
        }
        else if ((step == 2 || step == 3) && key == CONSOLE_KEY_DOWN) {
            printf("v ");
            step++;
            continue;
        }
        else if ((step == 4 || step == 6) && key == CONSOLE_KEY_LEFT) {
            printf("< ");
            step++;
            continue;
        }
        else if ((step == 5 || step == 7) && key == CONSOLE_KEY_RIGHT) {
            printf("> ");
            step++;
            continue;
        }
        else if (step == 8 && key == CONSOLE_KEY_B) {
            printf("B ");
            step++;
            continue;
        }
        else if (step == 9 && key == CONSOLE_KEY_A) {
            printf("A ");
            step++;
            continue;
        }
        else if (step == 10 && key == CONSOLE_KEY_ENTER) {
            printf("ENTER ");
            step++;
            break;
        }

        step = 0;
        printf("nope \n");
    }
    return 0;
}

void console_select_flush()
{
    memset(console_serial_tmp, 0, sizeof(console_serial_tmp));
    console_serial_len = 0;
    parsing_escape_code = 0;
    parsing_csi = 0;

    smc_get_events();
}

int console_select_poll()
{
    int ret = 0;

    serial_poll();
    console_serial_len = serial_in_read(console_serial_tmp);
    for (int i = 0; i < console_serial_len; i++) {
        //if (!menu_active) break;

        if (console_serial_tmp[i] == 0) continue;
        if (parsing_csi) {
            if (console_serial_tmp[i] >= '0' && console_serial_tmp[i] <= '9') {
                parsing_csi++;
                continue;
            }
            else if (console_serial_tmp[i] == 'A') {
                ret |= CONSOLE_KEY_UP;
                parsing_csi = 0;
                continue;
            }
            else if (console_serial_tmp[i] == 'B') {
                ret |= CONSOLE_KEY_DOWN;
                parsing_csi = 0;
                continue;
            }
            else if (console_serial_tmp[i] == 'C') {
                ret |= CONSOLE_KEY_RIGHT;
                parsing_csi = 0;
                continue;
            }
            else if (console_serial_tmp[i] == 'D') {
                ret |= CONSOLE_KEY_LEFT;
                parsing_csi = 0;
                continue;
            }
            else {
                parsing_csi = 0;
                continue;
            }
        }
        else if (parsing_escape_code)
        {
            if (parsing_escape_code == 1 && console_serial_tmp[i] == '[') {
                parsing_csi = 1;
                parsing_escape_code = 0;
                continue;
            }
            
            parsing_escape_code++;
        }
        else {
            switch (console_serial_tmp[i])
            {
            case '\033':
                parsing_escape_code = 1;
                break;
            case 'W':
            case 'w':
                ret |= CONSOLE_KEY_W;
                break;
            case 'S':
            case 's':
                ret |= CONSOLE_KEY_S;
                break;
            case 'A':
            case 'a':
                ret |= CONSOLE_KEY_A;
                break;
            case 'D':
            case 'd':
                ret |= CONSOLE_KEY_D;
                break;
            case '\b':
                ret |= CONSOLE_KEY_BACKSPACE;
                break;
            case 'Q':
            case 'q':
                ret |= CONSOLE_KEY_Q;
                break;
            case 'P':
            case 'p':
                ret |= CONSOLE_KEY_P;
                break;
            case 'B':
            case 'b':
                ret |= CONSOLE_KEY_B;
                break;
            case '\n':
            case '\r':
            case ' ':
                ret |= CONSOLE_KEY_ENTER;
                break;

            // Kinda hacky but whatever.
            case '\\':
                ret |= CONSOLE_KEY_INTCON;
                break;
            }
        }
    }

    u8 input = smc_get_events();

    //TODO: Double press to go back? Or just add "Back" options

    if(input & SMC_EJECT_BUTTON) ret |= CONSOLE_KEY_EJECT;
    if(input & SMC_POWER_BUTTON) ret |= CONSOLE_KEY_POWER;

    return ret;
}