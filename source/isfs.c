/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include "utils.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "isfs.h"
#include "crypto.h"

#include "ff.h"
#include "nand.h"
#include "sdmmc.h"
#include "sdcard.h"
#include "memory.h"

#include "isfshax.h"

//#define ISFS_DEBUG

#ifdef ISFS_DEBUG
#   define  ISFS_debug(f, arg...) printf("ISFS: " f, ##arg);
#else
#   define  ISFS_debug(f, arg...)
#endif

static bool initialized = false;

isfs_ctx isfs[4] = {
    [0]
    {
        .volume = 0,
        .name = "slc",
        .bank = NAND_BANK_SLC,
    },
    [1]
    {
        .volume = 1,
        .name = "slccmpt",
        .bank = NAND_BANK_SLCCMPT,
    },
    [2]
    {
        .volume = 2,
        .name = "redslc",
        .bank = 0x80000000 | 0,
    },
    [3]
    {
        .volume = 3,
        .name = "redslccmpt",
        .bank = 0x80000000 | 1,
    }
};

static int _isfs_num_volumes(void)
{
    return sizeof(isfs) / sizeof(isfs_ctx);
}

static isfs_ctx* _isfs_get_volume(int volume)
{
    if(volume < _isfs_num_volumes() && volume >= 0)
        return &isfs[volume];

    return NULL;
}

static u8 ecc_buf[ECC_BUFFER_ALLOC] ALIGNED(128);

static int _isfs_read_pages(isfs_ctx* ctx, void* buffer, u32 start, u32 pages)
{
    if(ctx->bank & 0x80000000) {
        inline u32 make_sector(u32 page) {
            return (page * PAGE_SIZE) / SDMMC_DEFAULT_BLOCKLEN;
        }

        u8 mbr[SDMMC_DEFAULT_BLOCKLEN] ALIGNED(32) = {0};
        if(sdcard_read(0, 1, mbr)) return -1;

        u8* part4 = &mbr[0x1EE];
        if(part4[0x4] != 0xAE) return -2;
        u32 lba = LD_DWORD(&part4[0x8]);

        u8 index = ctx->bank & 0xFF;
        u32 base = lba + (index * make_sector(NAND_MAX_PAGE));

        if(sdcard_read(base + make_sector(start), make_sector(pages), buffer))
            return -1;
    } else {
        nand_initialize(ctx->bank);

        u32 i, j;
        for(i = start, j = 0; i < start + pages; i++, j += PAGE_SIZE)
        {
            nand_read_page(i, buffer + j, ecc_buf);
            nand_wait();
            dc_invalidaterange(buffer + j, PAGE_SIZE);
            dc_invalidaterange(ecc_buf, ECC_BUFFER_ALLOC);
            nand_correct(i, buffer + j, ecc_buf);
        }
    }

    return 0;
}

static int _isfs_get_super_version(void* buffer)
{
    if(!memcmp(buffer, "SFFS", 4)) return 0;
    if(!memcmp(buffer, "SFS!", 4)) return 1;

    return -1;
}

static u32 _isfs_get_super_generation(void* buffer)
{
    return read32((u32)buffer + 4);
}

u16* _isfs_get_fat(isfs_ctx* ctx)
{
    return (u16*)&ctx->super[0x0C];
}

isfs_fst* _isfs_get_fst(isfs_ctx* ctx)
{
    return (isfs_fst*)&ctx->super[0x10000 + 0x0C];
}

static int _isfs_load_keys(isfs_ctx* ctx)
{
    switch(ctx->version) {
        case 0:
            memcpy(ctx->aes, otp.wii_nand_key, sizeof(ctx->aes));
            memcpy(ctx->hmac, otp.wii_nand_hmac, sizeof(ctx->hmac));
            break;
        case 1:
            memcpy(ctx->aes, otp.nand_key, sizeof(ctx->aes));
            memcpy(ctx->hmac, otp.nand_hmac, sizeof(ctx->hmac));
            break;
        default:
            printf("ISFS: Unknown super block version %u!\n", ctx->version);
            return -1;
    }

    return 0;
}

void isfs_print_fst(isfs_fst* fst)
{
    const char dir[4] = "?-d?";
    const char perm[3] = "-rw";

    u8 mode = fst->mode;
    char buffer[8] = {0};
    sprintf(buffer, "%c", dir[mode & 3]);
    sprintf(buffer, "%s%c%c", buffer, perm[(mode >> 6) & 1], perm[(mode >> 6) & 2]);
    mode <<= 2;
    sprintf(buffer, "%s%c%c", buffer, perm[(mode >> 6) & 1], perm[(mode >> 6) & 2]);
    mode <<= 2;
    sprintf(buffer, "%s%c%c", buffer, perm[(mode >> 6) & 1], perm[(mode >> 6) & 2]);
    mode <<= 2;

    printf("%s %02x %04x %04x %08lx (%04x %08lx)     %s\n", buffer,
            fst->attr, fst->uid, fst->gid, fst->size, fst->x1, fst->x3, fst->name);
}

static void _isfs_print_fst(isfs_fst* fst)
{
#ifdef ISFS_DEBUG
    isfs_print_fst(fst);
#endif
}

static void _isfs_print_dir(isfs_ctx* ctx, isfs_fst* fst)
{
    isfs_fst* root = _isfs_get_fst(ctx);

    if(fst->sib != 0xFFFF)
        _isfs_print_dir(ctx, &root[fst->sib]);

    _isfs_print_fst(fst);
}

static isfs_fst* _isfs_find_fst(isfs_ctx* ctx, isfs_fst* fst, const char* path);

static isfs_fst* _isfs_check_file(isfs_ctx* ctx, isfs_fst* fst, const char* path)
{
    char fst_name[sizeof(fst->name) + 1] = {0};
    memcpy(fst_name, fst->name, sizeof(fst->name));

    //ISFS_debug("file: %s vs %s\n", path, fst_name);

    if(!strcmp(fst_name, path))
        return fst;

    return NULL;
}

static isfs_fst* _isfs_check_dir(isfs_ctx* ctx, isfs_fst* fst, const char* path)
{
    isfs_fst* root = _isfs_get_fst(ctx);

    if(fst->sub != 0xFFFF)
        _isfs_print_dir(ctx, &root[fst->sub]);

    size_t size = strlen(path);
    const char* remaining = strchr(path, '/');
    if(remaining) size = remaining - path;

    if(size > sizeof(fst->name)) return NULL;

    char name[sizeof(fst->name) + 1] = {0};
    memcpy(name, path, size);

    char fst_name[sizeof(fst->name) + 1] = {0};
    memcpy(fst_name, fst->name, sizeof(fst->name));

    //ISFS_debug("dir: %s vs %s\n", name, fst_name);

    if(size == 0 || !strcmp(name, fst_name))
    {
        if(fst->sub != 0xFFFF && remaining != NULL && remaining[1] != '\0')
        {
            while(*remaining == '/') remaining++;
            return _isfs_find_fst(ctx, &root[fst->sub], remaining);
        }

        return fst;
    }

    return NULL;
}

static int _isfs_fst_get_type(const isfs_fst* fst)
{
    return fst->mode & 3;
}

static bool _isfs_fst_is_file(const isfs_fst* fst)
{
    return _isfs_fst_get_type(fst) == 1;
}

static bool _isfs_fst_is_dir(const isfs_fst* fst)
{
    return _isfs_fst_get_type(fst) == 2;
}

static isfs_fst* _isfs_find_fst(isfs_ctx* ctx, isfs_fst* fst, const char* path)
{
    isfs_fst* root = _isfs_get_fst(ctx);
    if(!fst) fst = root;

    if(fst->sib != 0xFFFF) {
        isfs_fst* result = _isfs_find_fst(ctx, &root[fst->sib], path);
        if(result) return result;
    }

    switch(_isfs_fst_get_type(fst)) {
        case 1:
            return _isfs_check_file(ctx, fst, path);
        case 2:
            return _isfs_check_dir(ctx, fst, path);
        default:
            printf("ISFS: Unknown mode! (%d)\n", _isfs_fst_get_type(fst));
            break;
    }

    return NULL;
}

char* _isfs_do_volume(const char* path, isfs_ctx** ctx)
{
    isfs_ctx* volume = NULL;

    if(!path) return NULL;
    const char* filename = strchr(path, ':');

    if(!filename) return NULL;
    if(filename[1] != '/') return NULL;

    char mount[sizeof(volume->name)] = {0};
    memcpy(mount, path, filename - path);

    for(int i = 0; i < _isfs_num_volumes(); i++)
    {
        volume = &isfs[i];
        if(strcmp(mount, volume->name)) continue;

        if(!volume->mounted) return NULL;

        *ctx = volume;
        return (char*)(filename + 1);
    }

    return NULL;
}

static int _isfs_load_super_range(isfs_ctx* ctx, u32 min_generation, u32 max_generation)
{
    int res = 0;

    const u32 start = 0x3E000;
    const u32 end = NAND_MAX_PAGE;
    const u32 size = 0x80;

    struct {
        u32 start;
        u32 generation;
        u8 version;
    } newest = {0};

    void* super = memalign(64, PAGE_SIZE);
    if(!super) return -1;

    for(u32 i = start; i < end; i += size)
    {
        res = _isfs_read_pages(ctx, super, i, 1);
        if(res) {
            ctx->mounted = false;
            free(super);
            return -2;
        }

        int version = _isfs_get_super_version(super);
        if(version < 0) continue;

        u32 generation = _isfs_get_super_generation(super);
        if(newest.start != 0 && generation < newest.generation) continue;
        if(generation >= max_generation) continue;
        if(generation < min_generation) continue;
        newest.start = i;
        newest.generation = generation;
        newest.version = version;
    }

    free(super);

    if(newest.start == 0)
    {
        printf("ISFS: Failed to find super block.\n");
        return -3;
    }

    ISFS_debug("Found super block (device=%s, version=%u, page=0x%lX, generation=0x%lX)\n",
            ctx->name, newest.version, newest.start, newest.generation);

    res = _isfs_read_pages(ctx, ctx->super, newest.start, size);
    if(res) {
        ctx->mounted = false;
        return -4;
    }

    ctx->generation = newest.generation;
    ctx->version = newest.version;

    _isfs_load_keys(ctx);

    ctx->mounted = true;
    return 0;
}

static int _isfs_load_super(isfs_ctx* ctx){
    u32 max_generation = 0xffffffff;
    int res = _isfs_load_super_range(ctx, ISFSHAX_GENERATION_FIRST, 0xffffffff);
    if(res<=0){
        if(read32((u32)ctx->super + ISFSHAX_INFO_OFFSET) == ISFSHAX_MAGIC){
            // Iisfshax was found, only look for non isfshax generations to mount
            max_generation = ISFSHAX_GENERATION_FIRST;
        }
    }
    return _isfs_load_super_range(ctx, 0, max_generation);
}

isfs_fst* isfs_stat(const char* path)
{
    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    if(!ctx || !path) return NULL;

    return _isfs_find_fst(ctx, NULL, path);
}

int isfs_open(isfs_file* file, const char* path)
{
    if(!file || !path) return -1;

    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    if(!ctx) return -2;

    isfs_fst* fst = _isfs_find_fst(ctx, NULL, path);
    if(!fst) return -3;

    if(!_isfs_fst_is_file(fst)) return -4;

    memset(file, 0, sizeof(isfs_file));
    file->volume = ctx->volume;
    file->fst = fst;

    file->cluster = fst->sub;
    file->offset = 0;

    return 0;
}

int isfs_close(isfs_file* file)
{
    if(!file) return -1;
    memset(file, 0, sizeof(isfs_file));

    return 0;
}

int isfs_seek(isfs_file* file, s32 offset, int whence)
{
    if(!file) return -1;

    isfs_ctx* ctx = _isfs_get_volume(file->volume);
    isfs_fst* fst = file->fst;
    if(!ctx || !fst) return -2;

    switch(whence) {
        case SEEK_SET:
            if(offset < 0) return -1;
            if(offset > fst->size) return -1;
            file->offset = offset;
            break;

        case SEEK_CUR:
            if(file->offset + offset > fst->size) return -1;
            if(offset + fst->size < 0) return -1;
            file->offset += offset;
            break;

        case SEEK_END:
            if(file->offset + offset > fst->size) return -1;
            if(offset + fst->size < 0) return -1;
            file->offset = fst->size + offset;
            break;
    }

    u16 sub = fst->sub;
    size_t size = file->offset;

    while(size > 8 * PAGE_SIZE) {
        sub = _isfs_get_fat(ctx)[sub];
        size -= 8 * PAGE_SIZE;
    }

    file->cluster = sub;

    return 0;
}

int isfs_read(isfs_file* file, void* buffer, size_t size, size_t* bytes_read)
{
    if(!file || !buffer) return -1;

    isfs_ctx* ctx = _isfs_get_volume(file->volume);
    isfs_fst* fst = file->fst;
    if(!ctx || !fst) return -2;

    aes_reset();
    aes_set_key((u8*)ctx->aes);

    if(size + file->offset > fst->size)
        size = fst->size - file->offset;

    size_t total = size;

    void* page_buf = memalign(256, 8 * PAGE_SIZE);
    if(!page_buf) return -3;

    while(size) {
        size_t work = min(8 * PAGE_SIZE, size);
        u32 pages = ((work + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) / PAGE_SIZE;

        //...why does _isfs_read_pages just do, nothing? if this is a lower value like 1?
        if (pages < 8) {
            pages = 8;
        }

        size_t pos = file->offset % (8 * PAGE_SIZE);
        size_t copy = (8 * PAGE_SIZE) - pos;
        if(copy > size) copy = size;

        //memset(page_buf, 0, 8 * PAGE_SIZE);

        _isfs_read_pages(ctx, page_buf, 8 * file->cluster, pages);

        aes_empty_iv();
        aes_decrypt((u8*)page_buf, (u8*)page_buf, (pages * PAGE_SIZE) / 0x10, 0);

        //printf("%x %x %x %x %x %x\n", file->offset, work, copy, pos, size, pages);
        //printf("%x %x %x %x %x\n", file->offset, work, copy, size, pages);
        memcpy(buffer, page_buf + pos, copy);

        file->offset += copy;
        buffer += copy;
        size -= copy;

        if((pos + copy) >= (8 * PAGE_SIZE))
            file->cluster = _isfs_get_fat(ctx)[file->cluster];
    }

    free(page_buf);

    *bytes_read = total;
    return 0;
}

int isfs_diropen(isfs_dir* dir, const char* path)
{
    if(!dir || !path) return -1;

    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    if(!ctx) return -2;

    isfs_fst* fst = _isfs_find_fst(ctx, NULL, path);
    if(!fst) return -3;

    if(!_isfs_fst_is_dir(fst)) return -4;
    if(fst->sub == 0xFFFF) return -2;

    isfs_fst* root = _isfs_get_fst(ctx);

    memset(dir, 0, sizeof(isfs_dir));
    dir->volume = ctx->volume;
    dir->dir = fst;
    dir->child = &root[fst->sub];

    return 0;
}

int isfs_dirread(isfs_dir* dir, isfs_fst** info)
{
    if(!dir) return -1;

    isfs_ctx* ctx = _isfs_get_volume(dir->volume);
    isfs_fst* fst = dir->dir;
    if(!ctx || !fst) return -2;

    isfs_fst* root = _isfs_get_fst(ctx);

    if(!info) {
        dir->child = &root[fst->sub];
        return 0;
    }

    *info = dir->child;

    if(dir->child != NULL) {
        if(dir->child->sib == 0xFFFF)
            dir->child = NULL;
        else
            dir->child = &root[dir->child->sib];
    }

    return 0;
}

int isfs_dirreset(isfs_dir* dir)
{
    return isfs_dirread(dir, NULL);
}

int isfs_dirclose(isfs_dir* dir)
{
    if(!dir) return -1;
    memset(dir, 0, sizeof(isfs_dir));

    return 0;
}

int isfs_init(void)
{
    if(initialized) return 0;

    for(int i = 0; i < _isfs_num_volumes(); i++)
    {
        isfs_ctx* ctx = &isfs[i];

        if(!ctx->super) ctx->super = memalign(64, 0x80 * PAGE_SIZE);
        if(!ctx->super) return -1;

        int res = _isfs_load_super(ctx);
        if(res) continue;

        int _isfsdev_init(isfs_ctx* ctx);
        _isfsdev_init(ctx);
    }

    initialized = true;

    return 0;
}

int isfs_fini(void)
{
    if(!initialized) return 0;

    for(int i = 0; i < _isfs_num_volumes(); i++)
    {
        isfs_ctx* ctx = &isfs[i];

        if(ctx->super) {
            free(ctx->super);
            ctx->super = NULL;
        }

        RemoveDevice(ctx->name);
        ctx->mounted = false;
    }

    initialized = false;

    return 0;
}

#include <sys/errno.h>
#include <sys/fcntl.h>

static void _isfsdev_fst_to_stat(const isfs_fst* fst, struct stat* st)
{
    memset(st, 0, sizeof(struct stat));

    st->st_uid = fst->uid;
    st->st_gid = fst->gid;

    st->st_mode = _isfs_fst_is_dir(fst) ? S_IFDIR : 0;
    st->st_size = fst->size;

    st->st_nlink = 1;
    st->st_rdev = st->st_dev;
    st->st_mtime = 0;

    //st->st_spare1 = fst->x1;
    //st->st_spare2 = fst->x3;
}

static int _isfsdev_stat_r(struct _reent* r, const char* file, struct stat* st)
{
    isfs_fst* fst = isfs_stat(file);
    if(!fst) {
        r->_errno = ENOENT;
        return -1;
    }

    _isfsdev_fst_to_stat(fst, st);

    return 0;
}

static ssize_t _isfsdev_read_r(struct _reent* r, void* fd, char* ptr, size_t len)
{
    isfs_file* fp = (isfs_file*) fd;

    size_t read = 0;
    int res = isfs_read(fp, ptr, len, &read);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return read;
}

static int _isfsdev_open_r(struct _reent* r, void* fileStruct, const char* path, int flags, int mode)
{
    isfs_file* fp = (isfs_file*) fileStruct;

    if (flags & (O_WRONLY | O_RDWR | O_CREAT | O_EXCL | O_TRUNC)) {
        r->_errno = ENOSYS;
        return -1;
    }

    int res = isfs_open(fp, path);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return 0;
}

static off_t _isfsdev_seek_r(struct _reent* r, void* fd, off_t pos, int dir)
{
    isfs_file* fp = (isfs_file*) fd;

    int res = isfs_seek(fp, (s32)pos, dir);

    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return fp->offset;
}

static int _isfsdev_close_r(struct _reent* r, void* fd)
{
    isfs_file* fp = (isfs_file*) fd;

    int res = isfs_close(fp);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return 0;
}

static DIR_ITER* _isfsdev_diropen_r(struct _reent* r, DIR_ITER* dirState, const char* path)
{
    isfs_dir* dir = (isfs_dir*) dirState->dirStruct;

    int res = isfs_diropen(dir, path);
    if(res) {
        r->_errno = EIO;
        return 0;
    }

    return dirState;
}

static int _isfsdev_dirreset_r(struct _reent* r, DIR_ITER* dirState)
{
    isfs_dir* dir = (isfs_dir*) dirState->dirStruct;

    int res = isfs_dirreset(dir);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return 0;
}

static int _isfsdev_dirnext_r(struct _reent* r, DIR_ITER* dirState, char* filename, struct stat* st)
{
    isfs_dir* dir = (isfs_dir*) dirState->dirStruct;

    isfs_fst* fst = NULL;
    int res = isfs_dirread(dir, &fst);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    if(!fst) return -1;

    _isfsdev_fst_to_stat(fst, st);

    memcpy(filename, fst->name, sizeof(fst->name));
    filename[sizeof(fst->name)] = '\0';

    return 0;
}

static int _isfsdev_dirclose_r(struct _reent* r, DIR_ITER* dirState)
{
    isfs_dir* dir = (isfs_dir*) dirState->dirStruct;

    int res = isfs_dirclose(dir);
    if(res) {
        r->_errno = EIO;
        return -1;
    }

    return 0;
}

int _isfsdev_init(isfs_ctx* ctx)
{
    devoptab_t* dotab = &ctx->devoptab;
    memset(dotab, 0, sizeof(devoptab_t));

    int _isfsdev_stub_r();

    dotab->name = ctx->name;
    dotab->deviceData = ctx;
    dotab->structSize = sizeof(isfs_file);
    dotab->dirStateSize = sizeof(isfs_dir);

    dotab->chdir_r = _isfsdev_stub_r;
    dotab->chmod_r = _isfsdev_stub_r;
    dotab->fchmod_r = _isfsdev_stub_r;
    dotab->fstat_r = _isfsdev_stub_r;
    dotab->fsync_r = _isfsdev_stub_r;
    dotab->ftruncate_r = _isfsdev_stub_r;
    dotab->link_r = _isfsdev_stub_r;
    dotab->mkdir_r = _isfsdev_stub_r;
    dotab->rename_r = _isfsdev_stub_r;
    dotab->rmdir_r = _isfsdev_stub_r;
    dotab->statvfs_r = _isfsdev_stub_r;
    dotab->unlink_r = _isfsdev_stub_r;
    dotab->write_r = _isfsdev_stub_r;

    dotab->close_r = _isfsdev_close_r;
    dotab->open_r = _isfsdev_open_r;
    dotab->read_r = _isfsdev_read_r;
    dotab->seek_r = _isfsdev_seek_r;
    dotab->stat_r = _isfsdev_stat_r;
    dotab->dirclose_r = _isfsdev_dirclose_r;
    dotab->diropen_r = _isfsdev_diropen_r;
    dotab->dirnext_r = _isfsdev_dirnext_r;
    dotab->dirreset_r = _isfsdev_dirreset_r;

    AddDevice(dotab);

    return 0;
}

int _isfsdev_stub_r(struct _reent *r)
{
    r->_errno = ENOSYS;
    return -1;
}

#include "smc.h"

#include <sys/errno.h>
#include <dirent.h>

void isfsdev_test_dir(void)
{
    int res = 0;

    const char* paths[] = {
        "slc:/sys/title/00050010/",
        "slc:/sys/title/00050010/1000400a/code/"
    };

    for(int i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
        gfx_clear(GFX_ALL, BLACK);
        const char* path = paths[i];

        printf("Reading directory %s...\n", path);

        DIR* dir = opendir(path);
        if(!dir) {
            printf("ISFS: opendir(path) returned %d.\n", errno);
            goto isfsdir_exit;
        }

        struct dirent* entry = NULL;
        struct stat info = {0};
        while((entry = readdir(dir)) != NULL) {
            char* filename = NULL;
            asprintf(&filename, "%s/%s", path, entry->d_name);

            res = stat(filename, &info);
            free(filename);
            if(res) {
                printf("ISFS: stat(%s) returned %d.\n", entry->d_name, errno);
                goto isfsdir_exit;
            }

            printf("%s, %s, size 0x%llX\n", entry->d_name, info.st_mode & S_IFDIR ? "dir" : "file", info.st_size);
        }

        res = closedir(dir);
        if(res) {
            printf("ISFS: closedir(dir) returned %d.\n", errno);
            goto isfsdir_exit;
        }

        if(i != (sizeof(paths) / sizeof(*paths)) - 1) {
            printf("Press POWER to continue.\n");
            smc_wait_events(SMC_POWER_BUTTON);
        }
    }

isfsdir_exit:
    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void isfsdev_test_file(void)
{
    int res;
    gfx_clear(GFX_ALL, BLACK);

    void* buffer = NULL;

    const char* path = "slc:/sys/title/00050010/1000400a/code/fw.img";
    printf("Dumping from %s...\n", path);

    FILE* file = fopen(path, "rb");
    if(!file) {
        printf("ISFS: fopen(path, \"rb\") returned %d.\n", errno);
        goto isfsfile_exit;
    }

    res = fseek(file, 0, SEEK_END);
    if(res) {
        printf("ISFS: fseek(file, 0, SEEK_END) returned %d.\n", res);
        goto isfsfile_exit;
    }
    size_t size = ftell(file);
    res = fseek(file, 0, SEEK_SET);
    if(res) {
        printf("ISFS: fseek(file, 0, SEEK_SET) returned %d.\n", res);
        goto isfsfile_exit;
    }

    printf("Size: 0x%X\n", size);

    buffer = malloc(size);
    int count = fread(buffer, size, 1, file);

    res = fclose(file);
    if(res) {
        printf("ISFS: fclose(file) returned %d.\n", res);
        goto isfsfile_exit;
    }

    if(count != 1) {
        printf("ISFS: fread(buffer, size, 1, file) returned %d.\n", errno);
        goto isfsfile_exit;
    }

    path = "sdmc:/slc-fw.img";
    printf("Dumping to %s...\n", path);
    file = fopen(path, "wb");
    if(!file) {
        printf("FATFS: fopen(path, \"wb\") returned %d.\n", errno);
        goto isfsfile_exit;
    }

    count = fwrite(buffer, size, 1, file);
    res = fclose(file);
    if(res) {
        printf("FATFS: fclose(file) returned %d.\n", res);
        goto isfsfile_exit;
    }

    if(count != 1) {
        printf("FATFS: fwrite(buffer, size, 1, file) returned %d.\n", errno);
        goto isfsfile_exit;
    }

isfsfile_exit:
    if(buffer) free(buffer);
    if(file) fclose(file);

    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void isfs_test(void)
{
    isfsdev_test_dir();
    isfsdev_test_file();
}
