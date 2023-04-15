/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _GFX_H
#define _GFX_H

#include "types.h"

#define RED    (0xFF0000FF)
#define GREEN  (0x00FF00FF)
#define BLACK  (0x00000000)
#define WHITE  (0xFFFFFFFF)

typedef enum {
	GFX_TV = 0,
	GFX_DRC,

	GFX_ALL,
} gfx_screen_t;

void gfx_init(void);
void gfx_draw_plot(gfx_screen_t screen, int x, int y, u32 color);
void gfx_clear(gfx_screen_t screen, u32 color);
void gfx_draw_string(gfx_screen_t screen, char* str, int x, int y, u32 color);

#ifdef MINUTE_BOOT1
static inline int printf(const char* fmt, ...)
{
    return 0;
}
#else
int printf(const char* fmt, ...);
#endif

#endif
