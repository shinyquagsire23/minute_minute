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
#include "hmac.h"

#include "ff.h"
#include "nand.h"
#include "sdmmc.h"
#include "sdcard.h"
#include "memory.h"
#include "rednand.h"

#include "isfshax.h"

// #define ISFS_DEBUG

#ifdef ISFS_DEBUG
#   define  ISFS_debug(f, arg...) printf("ISFS: " f, ##arg);
#else
#   define  ISFS_debug(f, arg...)
#endif

static u8 slc_cluster_buf[CLUSTER_SIZE] ALIGNED(NAND_DATA_ALIGN);
static u8 ecc_buf[ECC_BUFFER_ALLOC] ALIGNED(NAND_DATA_ALIGN);

static bool initialized = false;

isfs_ctx isfs[4] = {
    [ISFSVOL_SLC]
    {
        .volume = 0,
        .name = "slc",
        .bank = NAND_BANK_SLC,
        .super_count = 64,
    },
    [ISFSVOL_SLCCMPT]
    {
        .volume = 1,
        .name = "slccmpt",
        .bank = NAND_BANK_SLCCMPT,
        .super_count = 16,
    },
    [ISFSVOL_REDSLC]
    {
        .volume = 2,
        .name = "redslc",
        .bank = 0x80000000 | 0,
        .super_count = 64,
    },
    [ISFSVOL_REDSLCCMPT]
    {
        .volume = 3,
        .name = "redslccmpt",
        .bank = 0x80000000 | 1,
        .super_count = 16,
    }
};

static int _isfs_num_volumes(void)
{
    return sizeof(isfs) / sizeof(isfs_ctx);
}

isfs_ctx* isfs_get_volume(int volume)
{
    if(volume < _isfs_num_volumes() && volume >= 0)
        return &isfs[volume];

    return NULL;
}

static isfs_hdr* _isfs_get_hdr(isfs_ctx* ctx)
{
    return (isfs_hdr*)&ctx->super[0];
}

u16* _isfs_get_fat(isfs_ctx* ctx)
{
    return (u16*)&ctx->super[0x0C];
}

static int _isfs_super_check_slot(isfs_ctx *ctx, u32 index)
{
    u32 offs, cluster = CLUSTER_COUNT - (ctx->super_count - index) * ISFSSUPER_CLUSTERS;
    u16* fat = _isfs_get_fat(ctx);

    for (offs = 0; offs < ISFSSUPER_CLUSTERS; offs++)
        if (fat[cluster + offs] != FAT_CLUSTER_RESERVED)
            return -1;

    return 0;
}

static int _isfs_decrypt_cluster(const isfs_ctx* ctx, u8 *cluster_data){
    aes_reset();
    aes_set_key((u8*)ctx->aes);
    aes_empty_iv();
    aes_decrypt(cluster_data, cluster_data, CLUSTER_SIZE / ISFSAES_BLOCK_SIZE, 0);  
}

static int _isfs_read_sd(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *data){
    inline u32 make_sector(u32 page) {
        return (page * CLUSTER_SIZE) / SDMMC_DEFAULT_BLOCKLEN;
    }

    u8 index = ctx->bank & 0xFF;
    rednand_partition redpart = index?rednand.slccmpt:rednand.slc;

    if(!redpart.lba_length)
        return -1;

    if(sdcard_read(redpart.lba_start + make_sector(start_cluster), make_sector(cluster_count), data))
        return -1;

    if(flags & ISFSVOL_FLAG_ENCRYPTED){
        for (int p = 0; p < cluster_count; p++){
            _isfs_decrypt_cluster(ctx, data + p * CLUSTER_SIZE);
        }
    }
    return 0;
}

static int _nand_read_page_rawfile(u32 pageno, void *data, void *ecc, FIL* file){
#ifdef MINUTE_BOOT1
    return -128;
#else
    //ISFS_debug("ISFS: reading from file\n");
    u32 off = pageno * (PAGE_SIZE + PAGE_SPARE_SIZE);
    if(f_lseek(file, off) != FR_OK){
        ISFS_debug("ISFS: Error seeking file\n");
        return -1;
    }
    UINT br;
    if(f_read(file, data, PAGE_SIZE, &br) != FR_OK || br != PAGE_SIZE){
        ISFS_debug("ISFS: Error reading data from file\n");
        return -1;
    }
    if(f_read(file, ecc, PAGE_SPARE_SIZE, &br) != FR_OK || br != PAGE_SPARE_SIZE){
        ISFS_debug("ISFS: Error reading ecc from file\n");
        return -1;
    }
    return 0;
#endif //MINUTE_BOOT1
}

int isfs_read_volume(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *hmac_seed, void *data)
{
    if(ctx->bank & 0x80000000) {
        return _isfs_read_sd(ctx, start_cluster, cluster_count, flags, data);
    }

    u8 saved_hmacs[2][20] = {0}, hmac[20] = {0};
    u32 i, p;

    /* enable slc or slccmpt bank */
    if(!ctx->file)
        nand_initialize(ctx->bank);

    bool ecc_correctable = false;
    bool ecc_uncorrectable = false;
    bool hmac_error = false;
    bool hmac_partial = false;
    bool nand_error = false;

    /* read all requested clusters */
    for (i = 0; i < cluster_count; i++)
    {
        u32 cluster = start_cluster + i;
        u8 *cluster_data = (u8 *)data + i * CLUSTER_SIZE;
        u32 cluster_start = cluster * CLUSTER_PAGES;

        /* read cluster pages */
        for (p = 0; p < CLUSTER_PAGES; p++)
        {
            // make sure ECC fails, if read did nothing
            memset(ecc_buf, 0, ECC_BUFFER_ALLOC);
            /* attempt to read the page (and correct ecc errors) */
            int nand_error;
            if(ctx->file){
                nand_error = _nand_read_page_rawfile(cluster_start + p, &cluster_data[p * PAGE_SIZE], ecc_buf, ctx->file);
            } else {
                nand_error = nand_read_page(cluster_start + p, &cluster_data[p * PAGE_SIZE], ecc_buf);
                int correct = nand_correct(cluster_start + p, &cluster_data[p * PAGE_SIZE], ecc_buf);
                /* uncorrectable ecc error or other issues */
                if (correct < 0) {
                    ISFS_debug("Uncorrectable ECC ERROR\n");
                    ecc_uncorrectable = true;
                }

                /* ECC errors, a refresh might be needed */
                if (correct > 0){
                    ISFS_debug("Corrected ECC ERROR\n");
                    ecc_correctable = true;
                }
            }
                
            if(nand_error){
                ISFS_debug("NAND ERROR on read\n");
                nand_error = true;
            }

            /* page 6 and 7 store the hmac */
            if (p == 6)
            {
                memcpy(saved_hmacs[0], &ecc_buf[1], 20);
                memcpy(saved_hmacs[1], &ecc_buf[21], 12);
            }
            if (p == 7)
                memcpy(&saved_hmacs[1][12], &ecc_buf[1], 8);
        }

        /* decrypt cluster */
        if (flags & ISFSVOL_FLAG_ENCRYPTED)
            _isfs_decrypt_cluster(ctx, cluster_data);

    }

    if(nand_error)
        return ISFSVOL_ERROR_READ; 

    if(ecc_uncorrectable)
        return ISFSVOL_ERROR_ECC;

    /* verify hmac */
    if (flags & ISFSVOL_FLAG_HMAC)
    {
        hmac_ctx calc_hmac;
        int matched = 0;

        /* compute clusters hmac */
        hmac_init(&calc_hmac, ctx->hmac, 20);
        hmac_update(&calc_hmac, (const u8 *)hmac_seed, SHA_BLOCK_SIZE);
        hmac_update(&calc_hmac, (const u8 *)data, cluster_count * CLUSTER_SIZE);
        hmac_final(&calc_hmac, hmac);

        /* ensure at least one of the saved hmacs matches */
        matched += !memcmp(saved_hmacs[0], hmac, sizeof(hmac));
        matched += !memcmp(saved_hmacs[1], hmac, sizeof(hmac));

        if (matched == 1) {
            ISFS_debug("HMAC partital match\n");
            hmac_partial = true;
        }
        else if(!matched){
            ISFS_debug("HMAC error\n");
            return ISFSVOL_ERROR_HMAC;
        }
    }

    int rc = ISFSVOL_OK;
    if(ecc_correctable)
        rc |= ISFSVOL_ECC_CORRECTED;
    if(hmac_partial)
        rc |= ISFSVOL_HMAC_PARTIAL;
    return rc;
}

#ifdef NAND_WRITE_ENABLED
static int _isfs_write_sd(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *data){
    inline u32 make_sector(u32 page) {
        return (page * CLUSTER_SIZE) / SDMMC_DEFAULT_BLOCKLEN;
    }

    u8 index = ctx->bank & 0xFF;
    rednand_partition redpart = index?rednand.slccmpt:rednand.slc;

    if(!redpart.lba_length)
        return -1;

    if(sdcard_write(redpart.lba_start + make_sector(start_cluster), make_sector(cluster_count), data))
        return -1;

    if(flags & ISFSVOL_FLAG_ENCRYPTED){
        for (int p = 0; p < cluster_count; p++){
            _isfs_decrypt_cluster(ctx, data + p * CLUSTER_SIZE);
        }
    }
    return 0;
}

int isfs_write_volume(const isfs_ctx* ctx, u32 start_cluster, u32 cluster_count, u32 flags, void *hmac_seed, void *data)
{
    if(ctx->bank & 0x80000000) {
        return _isfs_write_sd(ctx, start_cluster, cluster_count, flags, data);
    }

    static u8 blockpg[BLOCK_PAGES][PAGE_SIZE] ALIGNED(NAND_DATA_ALIGN), blocksp[BLOCK_PAGES][PAGE_SPARE_SIZE];
    static u8 pgbuf[PAGE_SIZE] ALIGNED(NAND_DATA_ALIGN);
    u8 hmac[20] = {0};
    u32 b, p;

    /* enable slc or slccmpt bank */
    nand_initialize(ctx->bank);

    /* compute clusters hmac */
    if (flags & ISFSVOL_FLAG_HMAC)
    {
        hmac_ctx calc_hmac;
        hmac_init(&calc_hmac, ctx->hmac, 20);
        hmac_update(&calc_hmac, (const u8 *)hmac_seed, SHA_BLOCK_SIZE);
        hmac_update(&calc_hmac, (const u8 *)data, cluster_count * CLUSTER_SIZE);
        hmac_final(&calc_hmac, hmac);
    }

    /* setup clusters encryption */
    if (flags & ISFSVOL_FLAG_ENCRYPTED)
    {
        aes_reset();
        aes_set_key((u8*)ctx->aes);
        aes_empty_iv();
    }

    bool ecc_corrected = false;

    u32 startpage = start_cluster * CLUSTER_PAGES;
    u32 endpage = (start_cluster + cluster_count) * CLUSTER_PAGES;

    u32 startblock = start_cluster / BLOCK_CLUSTERS;
    u32 endblock = (start_cluster + cluster_count + BLOCK_CLUSTERS - 1) / BLOCK_CLUSTERS;

    /* process data in nand blocks */
    for (b = startblock; b < endblock; b++)
    {
        u32 firstblockpage = b * BLOCK_PAGES;

        /* prepare block */
        for (p = 0; p < 64; p++)
        {
            u32 curpage = firstblockpage + p;       /* current page */
            u32 clusidx = curpage % CLUSTER_PAGES;  /* index in cluster */

            /* if this page is unmodified, read it from nand */
            if ((curpage < startpage) || (curpage >= endpage))
            {
                ISFS_debug("Reading existing page\n");
                nand_read_page(curpage, blockpg[p], ecc_buf);
                if (nand_correct(curpage, blockpg[p], ecc_buf) < 0)
                    return ISFSVOL_ERROR_READ;
                memcpy(blocksp[p], ecc_buf, PAGE_SPARE_SIZE);
                continue;
            }

            /* place hmac in page 6 and 7 of a cluster */
            memset(blocksp[p], 0, PAGE_SPARE_SIZE);
            switch (clusidx)
            {
            case 6:
                memcpy(&blocksp[p][1], hmac, 20);
                memcpy(&blocksp[p][21], hmac, 12);
                break;
            case 7:
                memcpy(&blocksp[p][1], &hmac[12], 8);
                break;
            }

            /* encrypt or copy the data */
            u8 *srcdata = (u8*)data + (curpage - startpage) * PAGE_SIZE;
            if (flags & ISFSVOL_FLAG_ENCRYPTED)
                aes_encrypt(blockpg[p], srcdata, PAGE_SIZE / ISFSAES_BLOCK_SIZE, clusidx > 0);
            else
                memcpy(blockpg[p], srcdata, PAGE_SIZE);
        }
        ISFS_debug("Erase block\n");
        /* erase block */
        if (nand_erase_block(b * BLOCK_PAGES) < 0)
            return ISFSVOL_ERROR_ERASE;

        int write_error = 0;
        ISFS_debug("Writing\n");
        /* write block */
        for (p = 0; p < BLOCK_PAGES; p++)
            if (nand_write_page(firstblockpage + p, blockpg[p], blocksp[p]) < 0){
                printf("ISFS: Error writing page\n");
                write_error = ISFSVOL_ERROR_WRITE;
            }
        if(write_error)
            return write_error;

        /* check if pages should be verified after writing */
        if (!(flags & ISFSVOL_FLAG_READBACK))
            continue;

        ISFS_debug("Reading back\n");
        /* read back pages */
        for (p = 0; p < BLOCK_PAGES; p++)
        {
            memset(ecc_buf, 0xDEADBEEF, ECC_BUFFER_ALLOC);
            if(nand_read_page(firstblockpage + p, pgbuf, ecc_buf) < 0){
                printf("ISFS: Error reading back\n");
                return ISFSVOL_ERROR_READ;
            }
            int res = nand_correct(firstblockpage + p, pgbuf, ecc_buf);
            if(res<0)
                return ISFSVOL_ERROR_READ;
            if(res>0)
                ecc_corrected = true;

            /* page content doesn't match */
            if (memcmp(blockpg[p], pgbuf, PAGE_SIZE)){
                printf("ISFS: Read back data doesn't match\n");
                return ISFSVOL_ERROR_READBACK;
            }
            if (memcmp(&blocksp[p][1], &ecc_buf[1], 0x20)){
                printf("ISFS: Read back spare doesn't match\n");
                return ISFSVOL_ERROR_READBACK;
            }
        }
    }

    if(ecc_corrected)
        return ISFSVOL_ECC_CORRECTED;
    return ISFSVOL_OK;
}
#endif

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

static isfs_fst* _isfs_get_fst(isfs_ctx* ctx)
{
    return (isfs_fst*)&ctx->super[0x10000 + 0x0C];
}

int isfs_load_keys(isfs_ctx* ctx)
{
    otp_t *o = &otp;
    if(ctx->bank & 0x80000000 && redotp){
        printf("ISFS: using redotp\n");
        o = redotp;
    }

    switch(ctx->version) {
        case 0:
            memcpy(ctx->aes, o->wii_nand_key, sizeof(ctx->aes));
            memcpy(ctx->hmac, o->wii_nand_hmac, sizeof(ctx->hmac));
            break;
        case 1:
            memcpy(ctx->aes, o->nand_key, sizeof(ctx->aes));
            memcpy(ctx->hmac, o->nand_hmac, sizeof(ctx->hmac));
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

static isfs_fst* _isfs_find_fst(isfs_ctx* ctx, const char* path, void** parent){
    isfs_fst* root = _isfs_get_fst(ctx);
    if(parent)
        *parent = &root->sub;
    u16 next = root->sub;
    while(next!=0xFFFF){
        ISFS_debug("remaining path: %s\n", path);
        isfs_fst* fst = &root[next];
        while(*path== '/') path++;
        const char* remaining = strchr(path, '/');

        size_t size = remaining ? remaining - path : strlen(path);

        while((remaining && _isfs_fst_is_file(fst)) // skip files
                || (size < sizeof(fst->name) && fst->name[size]) //check if fst name length
                || memcmp(path, fst->name, size)){ //check name
            if(fst->sib == 0xFFFF)
                return NULL;
            if(parent)
                *parent = &fst->sib;
            fst = &root[fst->sib];
        }
        if(!remaining)
            return fst;
        if(parent)
            *parent = &fst->sub;
        next = fst->sub; // go down
        path = remaining;
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
    ISFS_debug("searching volume %s\n", mount);
    for(int i = 0; i < _isfs_num_volumes(); i++)
    {
        volume = &isfs[i];
        if(strcmp(mount, volume->name)) continue;

        if(!volume->mounted){
            ISFS_debug("volume %s not mounted\n", mount);
            return NULL;
        }
        *ctx = volume;
        return (char*)(filename + 1);
    }
    ISFS_debug("volume name not found\n");
    return NULL;
}

int isfs_read_super(isfs_ctx *ctx, void *super, int index)
{
    u32 cluster = CLUSTER_COUNT - (ctx->super_count - index) * ISFSSUPER_CLUSTERS;
    isfs_hmac_meta seed = { .cluster = cluster };
    return isfs_read_volume(ctx, cluster, ISFSSUPER_CLUSTERS, ISFSVOL_FLAG_HMAC, &seed, super);
}

#ifdef NAND_WRITE_ENABLED
int isfs_write_super(isfs_ctx *ctx, void *super, int index)
{
    u32 cluster = CLUSTER_COUNT - (ctx->super_count - index) * ISFSSUPER_CLUSTERS;
    isfs_hmac_meta seed = { .cluster = cluster };
    return isfs_write_volume(ctx, cluster, ISFSSUPER_CLUSTERS, ISFSVOL_FLAG_HMAC | ISFSVOL_FLAG_READBACK, &seed, super);
}
#endif

//not thread safe because of static buffer
int isfs_find_super(isfs_ctx* ctx, u32 min_generation, u32 max_generation, u32 *generation, u32 *version)
{
    struct {
        int index;
        u32 generation;
        u8 version;
    } newest = {-1, 0, 0};

    for(int i = 0; i < ctx->super_count; i++)
    {
        u32 cluster = CLUSTER_COUNT - (ctx->super_count - i) * ISFSSUPER_CLUSTERS;

        if(isfs_read_volume(ctx, cluster, 1, 0, NULL, slc_cluster_buf)<0)
            continue;

        int cur_version = _isfs_get_super_version(slc_cluster_buf);
        if(cur_version < 0) continue;

        u32 cur_generation = _isfs_get_super_generation(slc_cluster_buf);
        if((cur_generation < newest.generation) ||
           (cur_generation < min_generation) ||
           (cur_generation >= max_generation))
            continue;

        newest.index = i;
        newest.generation = cur_generation;
        newest.version = cur_version;
    }

    if(newest.index == -1)
    {
        ISFS_debug("Failed to find super block.\n");
        return -3;
    }

    ISFS_debug("Found super block (device=%s, version=%u, index=%d, generation=0x%lX)\n",
            ctx->name, newest.version, newest.index, newest.generation);

    if(generation) *generation = newest.generation;
    if(version) *version = newest.version;
    return newest.index;
}

static int _isfs_load_super_range(isfs_ctx* ctx, u32 min_generation, u32 max_generation)
{
    ctx->generation = max_generation;

    while((ctx->index = isfs_find_super(ctx, min_generation, ctx->generation, &ctx->generation, &ctx->version)) >= 0){
        isfs_load_keys(ctx);
        if(isfs_read_super(ctx, ctx->super, ctx->index) >= 0)
            break;
        else
            ISFS_debug("Reading superblock %d failed\n", ctx->index);
    }

    return (ctx->index >= 0) ? 0 : -1;
}

int isfs_load_super(isfs_ctx* ctx){
    u32 max_generation = 0xffffffff;
    ctx->isfshax = false;
    int res = _isfs_load_super_range(ctx, ISFSHAX_GENERATION_FIRST, 0xffffffff);
    if(res>=0){
        if(read32((u32)ctx->super + ISFSHAX_INFO_OFFSET) == ISFSHAX_MAGIC){
            // Iisfshax was found, only look for non isfshax generations to mount
            max_generation = ISFSHAX_GENERATION_FIRST;
            ctx->isfshax = true;
            isfshax_super *hax_super = (isfshax_super*)ctx->super;
            memcpy(ctx->isfshax_slots, hax_super->isfshax.slots, ISFSHAX_REDUNDANCY);
            printf("ISFShax detected\n");
        }
    }
    return _isfs_load_super_range(ctx, 0, max_generation);
}

#ifdef NAND_WRITE_ENABLED
int isfs_super_mark_slot(isfs_ctx *ctx, u32 index, u16 marker)
{
    u32 offs, cluster = CLUSTER_COUNT - (ctx->super_count - index) * ISFSSUPER_CLUSTERS;
    u16* fat = _isfs_get_fat(ctx);

    for (offs = 0; offs < ISFSSUPER_CLUSTERS; offs++)
        fat[cluster + offs] = marker;

    return 0;
}

bool isfs_is_isfshax_super(isfs_ctx* ctx, u8 index){
    if(!ctx->isfshax)
        return false;
    for(int i = 0; i<ISFSHAX_REDUNDANCY; i++){
        if(ctx->isfshax_slots[i] == index){
            return true;
        }
    }
    return false;
}


int isfs_commit_super(isfs_ctx* ctx)
{
    _isfs_get_hdr(ctx)->generation++;

    for(int i = 1; i <= ctx->super_count; i++)
    {
        u32 index = (ctx->index + i) % ctx->super_count;

        // should also be protected by the badblock list.
        if(isfs_is_isfshax_super(ctx, (u8)index))
            continue;

        if (_isfs_super_check_slot(ctx, index) < 0)
            continue;

        if (isfs_write_super(ctx, ctx->super, index) >= 0)
            return 0;

        isfs_super_mark_slot(ctx, index, FAT_CLUSTER_BAD);
        _isfs_get_hdr(ctx)->generation++;
    }

    return -1;
}
#endif //NAND_WRITE_ENABLED

isfs_fst* isfs_stat(const char* path)
{
    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    if(!ctx || !path) return NULL;

    return _isfs_find_fst(ctx, path, NULL);
}

#ifdef NAND_WRITE_ENABLED
int isfs_unlink(const char* path){
    if(!path)
        return -1;
    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    ISFS_debug("volume found: %p\n", ctx);
    if(!ctx)return -ENOENT;

    void *parent;
    isfs_fst* fst = _isfs_find_fst(ctx, path, &parent);
    ISFS_debug("fst found: %p\n", fst);
    if(!fst) return -ENOENT;

    if(!_isfs_fst_is_file(fst)) return -EISDIR;

    //parent might be unaligned
    memcpy(parent, &fst->sib, sizeof(fst->sib)); //remove from directory

    u16* fat = _isfs_get_fat(ctx);
    u16 cluster = fst->sub;
    while(cluster < 0xFFFB) {  
        u16 next_cluster = fat[cluster];
        fat[cluster] = 0xFFFE;
        cluster = next_cluster;
    }

    memset(fst, 0, sizeof(isfs_fst));

    int res = isfs_commit_super(ctx);
    if(res)
        return -EIO;
    return 0;
}
#endif //NAND_WRITE_ENABLED

int isfs_open(isfs_file* file, const char* path)
{
    if(!file || !path) return -1;

    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    ISFS_debug("volume found: %p\n", ctx);
    if(!ctx)return -2;

    isfs_fst* fst = _isfs_find_fst(ctx, path, NULL);
    ISFS_debug("fst found: %p\n", fst);
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

    isfs_ctx* ctx = isfs_get_volume(file->volume);
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

    isfs_ctx* ctx = isfs_get_volume(file->volume);
    isfs_fst* fst = file->fst;
    if(!ctx || !fst) return -2;

    if(size + file->offset > fst->size)
        size = fst->size - file->offset;

    size_t total = size;

    while(size) {
        size_t pos = file->offset % CLUSTER_SIZE;
        size_t copy = CLUSTER_SIZE - pos;
        if(copy > size) copy = size;

        if (isfs_read_volume(ctx, file->cluster, 1, ISFSVOL_FLAG_ENCRYPTED, NULL, slc_cluster_buf) < 0)
            return -4;
        memcpy(buffer, slc_cluster_buf + pos, copy);

        file->offset += copy;
        buffer += copy;
        size -= copy;

        if((pos + copy) >= CLUSTER_SIZE)
            file->cluster = _isfs_get_fat(ctx)[file->cluster];
    }

    *bytes_read = total;
    return 0;
}

int isfs_diropen(isfs_dir* dir, const char* path)
{
    if(!dir || !path) return -1;

    isfs_ctx* ctx = NULL;
    path = _isfs_do_volume(path, &ctx);
    if(!ctx) return -2;

    isfs_fst* fst = _isfs_find_fst(ctx, path, NULL);
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

    isfs_ctx* ctx = isfs_get_volume(dir->volume);
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

bool isfs_slc_has_isfshax_installed(void){
    isfs_init(ISFSVOL_SLC);
    return isfs[ISFSVOL_SLC].isfshax;
}

int isfs_init(unsigned int volume)
{
    if(volume>_isfs_num_volumes())
        return -3;
    isfs_ctx* ctx = &isfs[volume];
    if(ctx->mounted)
        return 1;
    printf("Mounting %s...\n", ctx->name);
    if(!ctx->super) ctx->super = memalign(NAND_DATA_ALIGN, 0x80 * PAGE_SIZE);
    if(!ctx->super) return -2;

    int res = isfs_load_super(ctx);
    if(res){
        free(ctx->super);
        printf("Failed to mount %s! Wrong OTP?\n", ctx->name);
        return -1;
    }
    ctx->mounted = true;

    int _isfsdev_init(isfs_ctx* ctx);
    _isfsdev_init(ctx);

    initialized = true;

    return 0;
}

int isfs_unmount(int volume){
    if(volume>_isfs_num_volumes())
        return -3;

    isfs_ctx* ctx = &isfs[volume];

    if(!ctx->mounted)
        return 1;

    if(ctx->super) {
        free(ctx->super);
        ctx->super = NULL;
    }

    RemoveDevice(ctx->name);
    ctx->mounted = false;
    ctx->isfshax = false;

    return 0;
}

int isfs_fini(void)
{
    if(!initialized) return 0;

    for(int i = 0; i < _isfs_num_volumes(); i++)
    {
        isfs_unmount(i);
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

#ifdef NAND_WRITE_ENABLED
static int _isfsdev_unlink_r(struct _reent* r, const char* path){
    int res = isfs_unlink(path);
    if(res) {
        r->_errno = -res;
        return -1;
    }
    return 0;
}
#endif

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
#ifdef NAND_WRITE_ENABLED
    dotab->unlink_r = _isfsdev_unlink_r;
#else
    dotab->unlink_r = _isfsdev_stub_r;
#endif

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
