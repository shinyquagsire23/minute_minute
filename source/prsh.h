/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _PRSH_H
#define _PRSH_H

#include "types.h"

#define PRSH_HEADER_MAGIC (0x50525348) // PRSH
#define PRST_MAGIC (0x50525354) // PRST

#define PRSHHAX_PAYLOAD_DST (0x00000048)
#define PRSHHAX_OTPDUMP_PTR (0x10009000)

#define PRSHHAX_OTP_MAGIC  (0x4F545044) // OTPD
#define PRSHHAX_FAIL_MAGIC (0x4641494C) // FAIL


typedef struct {
    char name[0x100];
    void* data;
    u32 size;
    u32 is_set;
    u8 unk[0x20];
} PACKED prsh_entry;

typedef struct {
    u32 checksum;
    u32 size;
    u32 is_set;
    u32 magic;
} PACKED prst_entry;

typedef struct {
    u32 checksum;
    u32 magic;
    u32 version;
    u32 size;
    u32 is_boot1;
    u32 total_entries;
    u32 entries;
    prsh_entry entry[];
} PACKED prsh_header;

typedef struct {
    u32 is_coldboot;
    u32 boot_flags;
    u32 boot_state;
    u32 boot_count;

    u32 field_10;
    u32 field_14; // set to 0x40000 for recovery
    u32 field_18;
    u32 field_1C;

    u32 field_20;
    u32 field_24;
    u32 os_id;      // os in dram (standy mode)
    u32 field_2C;

    u32 os_size;    // os in dram (standy mode)
    u32 os_address; // os in dram (standy mode)
    u32 boot1_main;
    u32 boot1_read;

    u32 boot1_verify;
    u32 boot1_decrypt;
    u32 boot0_main;
    u32 boot0_read;

    u32 boot0_verify;
    u32 boot0_decrypt;
} PACKED boot_info_t;

#define PRSH_FLAG_ISSET             (0x80000000)
#define PRSH_FLAG_WARM_BOOT         (0x40000000) // cleared on normal boots -- instructs MCP to not recreate mcp_launch_region, mcp_ramdisk_region, mcp_list_region, mcp_fs_cache_region
#define PRSH_FLAG_TITLES_ON_MLC     (0x20000000) // if unset, use HFIO/"SLC emulation"
#define PRSH_FLAG_10000000          (0x10000000) // unk
#define PRSH_FLAG_IOS_RELAUNCH      (0x08000000) // cleared on normal boots, set on fw.img reloads
#define PRSH_FLAG_HAS_BOOT_TIMES    (0x04000000) // set on normal boots
#define PRSH_FLAG_RETAIL            (0x02000000) // set if /sys/config/system.xml 'dev_mode' is 0
#define PRSH_FLAG_01000000          (0x01000000) // unk, can be read via MCP cmd
#define PRSH_FLAG_00800000          (0x00800000) // unk, has set/clear fn in MCP

void prsh_reset(void);
void prsh_print(void);
int prsh_exists_decrypted(void);
void prsh_init(void);
int prsh_get_entry(const char* name, void** data, size_t* size);
int prsh_set_entry(const char* name, void* data, size_t size);
int prsh_add_entry(const char* name, void* data, size_t size, prsh_entry** p_out);
void prsh_recompute_checksum();
int prsh_is_checksum_valid(prsh_header* header_in);
void prsh_decrypt();
void prsh_encrypt();

void print_bootinfo(boot_info_t * boot_info);
void prsh_copy_default_bootinfo(boot_info_t* boot_info);
void prsh_set_bootinfo();

void prsh_menu();

#endif
