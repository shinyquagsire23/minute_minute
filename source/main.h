/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _MAIN_H
#define _MAIN_H

#include "types.h"
#include "menu.h"

#include "dump.h"
#include "isfs.h"

void main_quickboot_fw(void);
void main_quickboot_patch(void);
void main_boot_fw(void);
void main_boot_ppc(void);
void main_shutdown(void);
void main_reset(void);
void main_reload(void);
void main_credits(void);
void main_get_crash(void);
void main_reset_crash(void);
void main_interactive_console(void);

extern menu menu_main;

#endif
