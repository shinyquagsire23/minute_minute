/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _DUMP_H
#define _DUMP_H

#include "types.h"

void dump_menu_show();

void dump_restore_seeprom(void);

int _dump_mlc(u32 base);
int _dump_slc(u32 base, u32 bank);
int _dump_slc_raw(u32 bank, int boot1_only);
int _dump_restore_mlc(u32 base);

int _dump_partition_rednand(void);
int _dump_copy_rednand(u32 slc_base, u32 slccmpt_base, u32 mlc_base);

void dump_slc_raw(void);
void dump_slccmpt_raw(void);
void dump_boot1_raw(void);
void dump_restore_slc_raw(void);
void dump_restore_slccmpt_raw(void);
void dump_restore_boot1_raw(void);

void dump_slc(void);
void dump_format_rednand(void);
void dump_restore_rednand(void);
void dump_seeprom_otp();
void dump_factory_log();

void dump_otp_via_prshhax(void);

void dump_sync_seeprom_boot1_versions(void);

#endif
