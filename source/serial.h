/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include "types.h"

void serial_fatal();
void serial_force_terminate();
void serial_send_u32(u32 val);
int serial_in_read(u8* out);
void serial_poll();
void serial_allow_zeros();
void serial_disallow_zeros();
void serial_send(u8 val);
void serial_line_inc();
void serial_clear();
void serial_line_noscroll();

#endif // __SERIAL_H__