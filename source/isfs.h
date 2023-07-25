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
#include <sys/iosupport.h>

#define ISFSSUPER_CLUSTERS  0x10
#define ISFSSUPER_SIZE      (ISFSSUPER_CLUSTERS * CLUSTER_SIZE)

typedef struct {
    int volume;
    const char name[0x10];
    const u32 bank;
    u8* super;
    u32 generation;
    u8 version;
    bool mounted;
    u32 aes[0x10/sizeof(u32)];
    u8 hmac[0x14];
    devoptab_t devoptab;
} isfs_ctx;

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

int isfs_init(void);
int isfs_fini(void);

void isfs_print_fst(isfs_fst* fst);
isfs_fst* isfs_stat(const char* path);

int isfs_open(isfs_file* file, const char* path);
int isfs_close(isfs_file* file);

int isfs_seek(isfs_file* file, s32 offset, int whence);
int isfs_read(isfs_file* file, void* buffer, size_t size, size_t* bytes_read);

char* _isfs_do_volume(const char* path, isfs_ctx** ctx);
isfs_fst* _isfs_get_fst(isfs_ctx* ctx);
u16* _isfs_get_fat(isfs_ctx* ctx);

void isfs_test(void);

#endif
