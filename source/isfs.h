/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _ISFS_H
#define _ISFS_H

#include "types.h"
#include "nand.h"
#include "fatfs/ff.h"
#include <sys/iosupport.h>

#define ISFSVOL_SLC             0
#define ISFSVOL_SLCCMPT         1
#define ISFSVOL_REDSLC             2
#define ISFSVOL_REDSLCCMPT         3

#define ISFSSUPER_CLUSTERS  0x10
#define ISFSSUPER_SIZE      (ISFSSUPER_CLUSTERS * CLUSTER_SIZE)
#define ISFSVOL_FLAG_HMAC       1
#define ISFSVOL_FLAG_ENCRYPTED  2
#define ISFSVOL_FLAG_READBACK   4

#define ISFSVOL_OK              0
#define ISFSVOL_ECC_CORRECTED   0x10
#define ISFSVOL_HMAC_PARTIAL    0x20
#define ISFSVOL_ERROR_WRITE     -0x10
#define ISFSVOL_ERROR_READ      -0x20
#define ISFSVOL_ERROR_ERASE     -0x30
#define ISFSVOL_ERROR_HMAC      -0x40
#define ISFSVOL_ERROR_READBACK  -0x50
#define ISFSVOL_ERROR_ECC       -0x60

#define ISFSAES_BLOCK_SIZE 0x10

#define FAT_CLUSTER_LAST        0xFFFB // last cluster within a chain
#define FAT_CLUSTER_RESERVED    0xFFFC // reserved cluster
#define FAT_CLUSTER_BAD         0xFFFD // bad block (marked at factory)
#define FAT_CLUSTER_EMPTY       0xFFFE // empty (unused / available) space


typedef struct {
    char name[12];
    u8 mode;
    u8 attr;
    u16 sub;
    u16 sib;
    u32 size;
    u16 x1;
    u16 uid;
    u16 gid;
    u32 x3;
} PACKED isfs_fst;

#include "isfshax.h"

typedef struct {
    int volume;
    const char name[0x10];
    const u32 bank;
    const u32 super_count;
    int index;
    u8* super;
    u32 generation;
    u32 version;
    bool mounted;
    bool isfshax;
    u8 isfshax_slots[ISFSHAX_REDUNDANCY];
    u32 aes[0x10/sizeof(u32)];
    u8 hmac[0x14];
    devoptab_t devoptab;
    FIL* file;
} isfs_ctx;

typedef struct {
    int volume;
    isfs_fst* fst;
    size_t offset;
    u16 cluster;
} isfs_file;

typedef struct {
    int volume;
    isfs_fst* dir;
    isfs_fst* child;
} isfs_dir;

typedef struct {
    u16 x1;
    u16 uid;
    char name[0x0C];
    u32 iblk;
    u32 ifst;
    u32 x3;
    u8 pad0[0x24];
} isfs_hmac_data;
_Static_assert(sizeof(isfs_hmac_data) == 0x40, "isfs_hmac_data size must be 0x40!");

typedef struct {
    u8 pad0[0x12];
    u16 cluster;
    u8 pad1[0x2b];
} isfs_hmac_meta;
_Static_assert(sizeof(isfs_hmac_meta) == 0x40, "isfs_hmac_meta size must be 0x40!");

typedef struct isfs_hdr {
    char magic[4];
    u32 generation;
    u32 x1;
} PACKED isfs_hdr;
_Static_assert(sizeof(isfs_hdr) == 0xC, "isfs_hdr size must be 0xC!");

int isfs_init(unsigned int volume);
int isfs_unmount(int volume);
int isfs_fini(void);
int isfs_load_keys(isfs_ctx* ctx);

void isfs_print_fst(isfs_fst* fst);
isfs_fst* isfs_stat(const char* path);

int isfs_open(isfs_file* file, const char* path);
int isfs_close(isfs_file* file);

int isfs_seek(isfs_file* file, s32 offset, int whence);
int isfs_read(isfs_file* file, void* buffer, size_t size, size_t* bytes_read);

char* _isfs_do_volume(const char* path, isfs_ctx** ctx);
isfs_ctx* isfs_get_volume(int volume);
int isfs_read_volume(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *hmac_seed, void *data);
int isfs_read_super(isfs_ctx *ctx, void *super, int index);
bool isfs_is_isfshax_super(isfs_ctx* ctx, u8 index);
int isfs_load_super(isfs_ctx* ctx);
#ifdef NAND_WRITE_ENABLED
int isfs_write_volume(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *hmac_seed, void *data);
int isfs_write_super(isfs_ctx *ctx, void *super, int index);
int isfs_commit_super(isfs_ctx* ctx);
int isfs_super_mark_slot(isfs_ctx *ctx, u32 index, u16 marker);
#endif

u16* _isfs_get_fat(isfs_ctx* ctx);

bool isfs_slc_has_isfshax_installed(void);

void isfs_test(void);

#endif
