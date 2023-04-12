/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include "utils.h"
#include "gfx.h"
#include "gpio.h"
#include "latte.h"

#include <stdarg.h>

#if defined(CAN_HAZ_USBGECKO) && !defined(LOADER) && !defined(NDEBUG)
static char ascii(char s) {
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(const void *d, int len) {
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16) {
    printf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len) printf("   ");
      else printf("%02x ",data[off+i]);

    printf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) printf(" ");
      else printf("%c",ascii(data[off+i]));
    printf("\n");
  }
}
#endif

void udelay(u32 d)
{
    // should be good to max .2% error
    u32 ticks = d * 19 / 10;

    if(ticks < 2)
        ticks = 2;

    u32 now = read32(LT_TIMER);

    u32 then = now + ticks;

    if(then < now) {
        while(read32(LT_TIMER) >= now);
        now = read32(LT_TIMER);
    }

    while(now < then) {
        now = read32(LT_TIMER);
    }
}

void panic(u8 v)
{
    while(true) {
        //debug_output(v);
        //set32(HW_GPIO1BOUT, BIT(GP_SLOTLED));
        //udelay(500000);
        //debug_output(0);
        //clear32(HW_GPIO1BOUT, BIT(GP_SLOTLED));
        //udelay(500000);
    }
}

