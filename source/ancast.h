/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _ANCAST_H
#define _ANCAST_H

#include "types.h"

#include "sha.h"
#include "prsh.h"

typedef struct {
    u16 unk1;
    u8 unk2;
    u8 unk3;
    u32 device;
    u32 type;
    u32 body_size;
    u32 body_hash[SHA_HASH_WORDS];
    u32 version;
    u8 padding[0x38];
} ancast_header;

typedef struct {
    u8 magic[8];
    u8 magic_device[8];
    u8 magic_prsh[8];
    boot_info_t boot_info;
    u32 start_time;
} boot1_passalong_info;

u32 ancast_iop_load(const char* path);
u32 ancast_ppc_load(const char* path);

u32 ancast_iop_load_from_raw_sector(int sector_idx);
u32 ancast_iop_load_from_memory(void* ancast_mem);
u32 ancast_patch_load(const char* fn_ios, const char* fn_patch, const char* plugins_fpath, bool rednand);

u32 ancast_plugins_load(const char* plugins_fpath, bool rednand);

extern uintptr_t ancast_plugins_base;

// Used for patches on IOS boot, and the passalong magic otherwise.
#define ALL_PURPOSE_TMP_BUF (0x00800000)
#define BOOT1_PASSALONG ((boot1_passalong_info*)ALL_PURPOSE_TMP_BUF)

// TODO: determine this based on plugins
#define CARVEOUT_SZ (0x400000)
#define MAGIC_PLUG (0x504C5547)
#define MAX_PLUGINS (256)

#define RAMDISK_END_ADDR (0x28000000)
#define MAGIC_PLUG_ADDR (RAMDISK_END_ADDR-8)

#define PASSALONG_MAGIC_BOOT1 ("MINTBT01")
#define PASSALONG_MAGIC_PRSH_ENCRYPTED ("MIECPRSH")
#define PASSALONG_MAGIC_PRSH_DECRYPTED ("MIDEPRSH")
#define PASSALONG_MAGIC_DEVICE_SLC ("MIDEVSLC")
#define PASSALONG_MAGIC_DEVICE_SD ("MIDEVESD")

#define ANCAST_MAGIC (0xEFA282D9l)
#define ANCAST_TARGET_IOP (0x02)
#define ANCAST_TARGET_PPC (0x01)

#define ANCAST_PPC_WIIU (0x01)
#define ANCAST_PPC_VWII (0x03)

#define ANCAST_CONSOLE_TYPE_PROD (0x2)
#define ANCAST_CONSOLE_TYPE_DEV  (0x1)

#define IPX_ELF_MAGIC (0x7F454C46)
#define IPX_DATA_MAGIC (0x44415441)
#define IPX_NORMAL_EHDR_SIZE (0x200)
#define IPX_ENTRY_HDR_SIZE (0x20)
#define IPX_DATA_START (IPX_NORMAL_EHDR_SIZE+IPX_ENTRY_HDR_SIZE)

#endif
