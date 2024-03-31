/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __MLC_H__
#define __MLC_H__

#include "bsdtypes.h"
#include "sdmmc.h"

int mlc_init(void);
void mlc_exit(void);
void mlc_irq(void);

void mlc_attach(sdmmc_chipset_handle_t handle);
void mlc_needs_discover(void);
int mlc_wait_data(void);

int mlc_select(void);
int mlc_check_card(void);
int mlc_ack_card(void);
u32 mlc_get_sectors(void);

int mlc_read(u32 blk_start, u32 blk_count, void *data);
int mlc_write(u32 blk_start, u32 blk_count, void *data);

int mlc_start_read(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf);
int mlc_end_read(struct sdmmc_command* cmdbuf);

int mlc_start_write(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf);
int mlc_end_write(struct sdmmc_command* cmdbuf);

int mlc_erase(void);

#endif
