/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _EXI_H
#define _EXI_H

#include "types.h"

void exi0_write32(u32 addr, u32 val);
u32 exi0_read32(u32 addr);
void exi1_write32(u32 addr, u32 val);

#endif // _EXI_H