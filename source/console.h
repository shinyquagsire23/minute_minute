/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "types.h"

//TODO: TV screen defaults?
#define MAX_LINES 44
#define MAX_LINE_LENGTH 100
#define CONSOLE_X 10
#define CONSOLE_Y 10
#define CONSOLE_WIDTH (854-15)
#define CONSOLE_HEIGHT (480-15)

#define CONSOLE_TV_X 10
#define CONSOLE_TV_Y 10
#define CONSOLE_TV_WIDTH (1280-20)
#define CONSOLE_TV_HEIGHT (720-20)

#define CONSOLE_KEY_UP     (1)
#define CONSOLE_KEY_DOWN   (2)
#define CONSOLE_KEY_LEFT   (4)
#define CONSOLE_KEY_RIGHT  (8)
#define CONSOLE_KEY_ENTER  (0x10)
#define CONSOLE_KEY_EJECT  (0x20)
#define CONSOLE_KEY_POWER  (0x40)
#define CONSOLE_KEY_INTCON (0x80)
#define CONSOLE_KEY_Q      (0x100)
#define CONSOLE_KEY_P      (0x200)
#define CONSOLE_KEY_BACKSPACE (0x400)
#define CONSOLE_KEY_A      (0x800)
#define CONSOLE_KEY_B      (0x1000)
#define CONSOLE_KEY_W      (0x2000)
#define CONSOLE_KEY_S      (0x4000)
#define CONSOLE_KEY_D      (0x8000)

void console_init();
void console_show();
void console_flush();
void console_add_text(char* str);

void console_set_xy(int x, int y);
void console_get_xy(int *x, int *y);
void console_set_wh(int width, int height);

void console_set_text_color(int color);
int console_get_text_color();
void console_set_background_color(int color);
int console_get_background_color();

void console_set_border_color(int color);
int console_get_border_color();
void console_set_border_width(int width);
int console_get_border_width(int width);

int console_konami_code();
void console_power_or_eject_to_return();
void console_power_to_exit();
void console_power_to_continue();
int console_abort_confirmation_power_no_eject_yes();
int console_abort_confirmation_power_exit_eject_continue();
int console_abort_confirmation_power_skip_eject_dump();
void console_select_flush();
int console_select_poll();

#endif
