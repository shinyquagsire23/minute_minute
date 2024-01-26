/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _PPC_H
#define _PPC_H

void ppc_hang(void);
void ppc_prepare(void);
void ppc_jump(u32 entry);
void ppc_test(u32 val);
void ppc_dump_bootrom_otp(void);

#endif

