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
#define CONSOLE_WIDTH 839
#define CONSOLE_HEIGHT 465
#define MAX_LINES 44
#define MAX_LINE_LENGTH 100
#define CONSOLE_X 10
#define CONSOLE_Y 10

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

#endif
