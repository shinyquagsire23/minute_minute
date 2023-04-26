/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "menu.h"

#include "types.h"
#include "utils.h"
#include "gfx.h"
#include "console.h"
#include "serial.h"
#include <stdio.h>

#include "smc.h"

menu* __menu;
menu *menu_chain[100];
int opened_menus = 0;

bool menu_active = true;
int _menu_state = -1;

void main_interactive_console(void);

void menu_set_state(int state)
{
    _menu_state = state;
}

int menu_get_state()
{
    return _menu_state;
}

void menu_init(menu* new_menu)
{
    char serial_tmp[256];
    int serial_len = 0;
    menu_set_state(0); // Set state to in-menu.

    smc_get_events(); // Eat all existing events

    __menu = new_menu;
    __menu->selected = 0;
    __menu->showed = 0;
    __menu->selected_showed = 0;
    menu_draw();

    int parsing_escape_code = 0;
    int parsing_csi = 0;

    menu_active = true;

    while(menu_active)
    {
        menu_show();

        serial_poll();
        serial_len = serial_in_read(serial_tmp);
        for (int i = 0; i < serial_len; i++) {
            if (serial_tmp[i] == 0) continue;
            if (parsing_csi) {
                if (serial_tmp[i] >= '0' && serial_tmp[i] <= '9') {
                    parsing_csi++;
                    continue;
                }
                else if (serial_tmp[i] == 'A') {
                    menu_prev_selection();
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'B') {
                    menu_next_selection();
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'C') {
                    menu_next_jump();
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'D') {
                    menu_prev_jump();
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
                if (parsing_escape_code == 1 && serial_tmp[i] == '[') {
                    parsing_csi = 1;
                    parsing_escape_code = 0;
                    continue;
                }
                
                parsing_escape_code++;
            }
            else {
                switch (serial_tmp[i])
                {
                case '\033':
                    parsing_escape_code = 1;
                    break;
                case 'W':
                case 'w':
                    menu_prev_selection();
                    break;
                case 'S':
                case 's':
                    menu_next_selection();
                    break;
                case 'A':
                case 'a':
                    menu_prev_jump();
                    break;
                case 'D':
                case 'd':
                    menu_next_jump();
                    break;
                case '\n':
                case '\r':
                case ' ':
                    menu_select();
                    break;

                // Kinda hacky but whatever.
                case '\\':
                    for (int j = 0; j < __menu->entries; j++) {
                        if (__menu->option[j].callback == main_interactive_console) {
                            __menu->selected = j;
                            menu_select();
                        }
                    }
                    break;
                }
            }
        }

        u8 input = smc_get_events();

        //TODO: Double press to go back? Or just add "Back" options

        if(input & SMC_EJECT_BUTTON) menu_select();
        if(input & SMC_POWER_BUTTON) menu_next_selection();
    }
}

void menu_draw()
{
    char item_buffer[100] = {0};

    console_init();
    console_add_text(__menu->title);
    console_add_text("");

    for (int i = 0; i < __menu->subtitles; i++) {
        console_add_text(__menu->subtitle[i]);
    }

    console_add_text("");

    for(int i = 0; i < __menu->entries; i++)
    {
        char selected_char = ' ';
        if (gfx_is_currently_headless()) {
            if (i == __menu->selected) {
                selected_char = '>';
            }
        }

        sprintf(item_buffer, "%c%s", selected_char, __menu->option[i].text);
        console_add_text(item_buffer);
    }
}

void menu_show()
{
    int i = 0, x = 0, y = 0;
    console_get_xy(&x, &y);
    if(!__menu->showed)
    {
        console_show();
        __menu->showed = 1;
    }

    // Update cursor.
    for(i = 0; i < __menu->entries; i++) {
        gfx_draw_string(GFX_DRC, i == __menu->selected ? ">" : " ", x + CHAR_WIDTH, (i+3+__menu->subtitles) * CHAR_WIDTH + y + CHAR_WIDTH * 2, GREEN);
    }
    if (gfx_is_currently_headless() && !__menu->selected_showed) {
        menu_draw();
        console_show();
        __menu->selected_showed = 1;
    }
}

void menu_next_selection()
{
    if(__menu->selected + 1 < __menu->entries)
        __menu->selected++;
    else
        __menu->selected = 0;
    __menu->selected_showed = 0;
}

void menu_prev_selection()
{
    if(__menu->selected > 0)
        __menu->selected--;
    else
        __menu->selected = __menu->entries - 1;
    __menu->selected_showed = 0;
}

void menu_next_jump()
{
    if(__menu->selected + 5 < __menu->entries)
        __menu->selected += 5;
    else
        __menu->selected = __menu->entries - 1;
    __menu->selected_showed = 0;
}

void menu_prev_jump()
{
    if(__menu->selected > 5)
        __menu->selected -= 5;
    else
        __menu->selected = 0;
    __menu->selected_showed = 0;
}


void menu_select()
{
    if(__menu->option[__menu->selected].callback != NULL)
    {
        menu_chain[opened_menus++] = __menu;
        menu_set_state(1); // Set menu state to in callback.
        __menu->option[__menu->selected].callback();
        menu_set_state(0); // Return to in-menu state.
        if (opened_menus >= 1) {
            menu_init(menu_chain[--opened_menus]);
        }
    }
}

void menu_close()
{
    menu_active = false;
    if (opened_menus >= 1) {
        --opened_menus;
    }
}

void menu_reset()
{
    menu_active = false;
    opened_menus = 0;
}
