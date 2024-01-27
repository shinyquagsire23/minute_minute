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
    menu_set_state(0); // Set state to in-menu.

    console_select_flush(); // Eat all existing events

    console_init();
    __menu = new_menu;
    __menu->selected = 0;
    __menu->showed = 0;
    __menu->selected_showed = 0;
    menu_draw();

    menu_active = true;
    console_select_flush(); // Eat all existing events

    while(menu_active)
    {
        menu_show();

        int console_input = console_select_poll();
        int do_select = 0;

        if ((console_input & CONSOLE_KEY_UP) || (console_input & CONSOLE_KEY_W)) {
            menu_prev_selection();
        }
        if ((console_input & CONSOLE_KEY_DOWN) || (console_input & CONSOLE_KEY_S)) {
            menu_next_selection();
        }
        if ((console_input & CONSOLE_KEY_LEFT) || (console_input & CONSOLE_KEY_A)) {
            menu_prev_jump();
        }
        if ((console_input & CONSOLE_KEY_RIGHT) || (console_input & CONSOLE_KEY_D)) {
            menu_next_jump();
        }
        if (console_input & CONSOLE_KEY_ENTER) {
            do_select = 1;
        }
        
        if ((console_input & CONSOLE_KEY_EJECT) || (console_input & CONSOLE_KEY_P)) {
            do_select = 1;
        }
        else if ((console_input & CONSOLE_KEY_POWER) || (console_input & CONSOLE_KEY_Q)) {
            menu_next_selection();
        }
        
        if (console_input & CONSOLE_KEY_INTCON) {
            // Kinda hacky but whatever.
            for (int j = 0; j < __menu->entries; j++) {
                if (__menu->option[j].callback == main_interactive_console) {
                    __menu->selected = j;
                    do_select = 1;
                }
            }
        }

        if (do_select)
            menu_select();
    }
}

void menu_draw()
{
    char tmp[128];
    char item_buffer[100] = {0};

    if (__menu->showed) {
        serial_line_noscroll();
    }
    console_flush();
    snprintf(tmp, sizeof(tmp), " %s", __menu->title);
    console_add_text(tmp);
    console_add_text("");

    for (int i = 0; i < __menu->subtitles; i++) {
        snprintf(tmp, sizeof(tmp), " %s", __menu->subtitle[i]);
        console_add_text(tmp);
    }

    console_add_text("");

    for(int i = 0; i < __menu->entries; i++)
    {
        char selected_char = ' ';
        char selected_char2 = ' ';
        //if (gfx_is_currently_headless()) 
        {
            if (i == __menu->selected) {
                selected_char = '>';
                selected_char2 = '<';
            }
        }

        sprintf(item_buffer, "%c %s %c", selected_char, __menu->option[i].text, selected_char2);
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
        gfx_draw_string(GFX_TV, i == __menu->selected ? ">" : " ", x + CHAR_WIDTH, (i+3+__menu->subtitles) * CHAR_WIDTH + y + CHAR_WIDTH * 2, GREEN);
    }
    if (/*gfx_is_currently_headless() && */!__menu->selected_showed) 
    {
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
