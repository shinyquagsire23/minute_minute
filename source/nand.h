/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __NAND_H__
#define __NAND_H__

#include "types.h"

#define NAND_DATA_ALIGN     128

#define PAGE_SIZE        (2048)
#define PAGE_SPARE_SIZE  (64)
#define PAGE_COUNT       (0x40000)
#define ECC_BUFFER_SIZE  (PAGE_SPARE_SIZE+16)
#define ECC_BUFFER_ALLOC (PAGE_SPARE_SIZE+32)
#define BLOCK_PAGES      (64)
#define BLOCK_CLUSTERS   (8)
#define NAND_MAX_PAGE    (0x40000)
#define BOOT1_MAX_PAGE   (0x40)
#define STATUS_BUF_SIZE  (0x40)

#define NAND_BANK_SLCCMPT (0x00000001)
#define NAND_BANK_SLC     (0x00000002)

#define NAND_CMD_EXEC (1<<31)

#define CLUSTER_PAGES       8
#define CLUSTER_SIZE        (PAGE_SIZE * CLUSTER_PAGES)
#define CLUSTER_COUNT       (PAGE_COUNT / CLUSTER_PAGES)

void nand_irq(void);

void nand_send_command(u32 command, u32 bitmask, u32 flags, u32 num_bytes);
int nand_reset(u32 bank);
void nand_get_id(u8 *);
void nand_get_status(u8 *);
int nand_read_page(u32 pageno, void *data, void *ecc);
int nand_write_page_raw(u32 pageno, void *data, void *ecc);
int nand_write_page(u32 pageno, void *data, void *ecc);
int nand_erase_block(u32 pageno);
void nand_wait(void);

#define NAND_ECC_OK 0
#define NAND_ECC_CORRECTED 1
#define NAND_ECC_UNCORRECTABLE -1

int nand_correct(u32 pageno, void *data, void *ecc);
void nand_initialize(u32 bank);
void nand_create_ecc(void* in_data, void* spare_out);

#endif

