/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _MENU_H
#define _MENU_H

#include "types.h"
#include "console.h"

extern bool menu_active;

typedef struct {
    char* text;
    void(* callback)();
} menu_item;

typedef struct {
    char* title;
    char* subtitle[MAX_LINES - 1];
    int subtitles;
    menu_item option[MAX_LINES - 1];
    int entries;
    int selected;
    bool selected_showed;
    bool showed;
} menu;

void menu_init(menu* menu);
void menu_draw();
void menu_show();
void menu_next_selection();
void menu_prev_selection();
void menu_next_jump();
void menu_prev_jump();
void menu_select();
void menu_close();
void menu_reset();

void menu_set_state(int state);
int menu_get_state();

#endif
