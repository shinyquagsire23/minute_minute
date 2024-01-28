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

#define EXI_START_TRANSFER (1)

#define EXI_TRANSFER_TYPE_R (0x0)
#define EXI_TRANSFER_TYPE_W (0x4)

#define EXI_TRANSFER_LENGTH(n) ((n-1)<<4)

void exi_init(void);

void exi0_write32(u32 addr, u32 val);
u32 exi0_read32(u32 addr);
void exi1_write32(u32 addr, u32 val);

#endif // _EXI_H