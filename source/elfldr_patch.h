/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */
#ifndef __ELFPTCH_H__
#define __ELFPTCH_H__

extern void elfldr_start();
extern void elfldr_end();
#define elfldr_patch ((const char*)elfldr_start)
#define elfldr_patch_len (((const char*)elfldr_end) - elfldr_patch)

#endif