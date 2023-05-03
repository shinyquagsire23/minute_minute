/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "ancast.h"

#include "types.h"
#include "gfx.h"
#include "utils.h"
#include "memory.h"

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "sha.h"
#include "crypto.h"
#include "smc.h"
#include "sdcard.h"
#include "serial.h"
#include "elfldr_patch.h"

char sd_read_buffer[0x200] ALIGNED(0x20);

typedef struct {
    ancast_header header;
    size_t header_size;
    FILE* file;
    const char* path;
    size_t size;
    void* load;
    void* body;
    u32 sector_idx;
    void* memory_load;
} ancast_ctx;

int ancast_fini(ancast_ctx* ctx);

int ancast_init(ancast_ctx* ctx, const char* path)
{
    if(!ctx || !path) return -1;
    memset(ctx, 0, sizeof(ancast_ctx));

    ctx->path = path;
    ctx->file = fopen(path, "rb");
    if(!ctx->file) {
        printf("ancast: failed to open %s (%d).\n", path, errno);
        return errno;
    }

    fseek(ctx->file, 0, SEEK_END);
    ctx->size = ftell(ctx->file);
    fseek(ctx->file, 0, SEEK_SET);

    u8 buffer[0x200] = {0};
    fread(buffer, min(sizeof(buffer), ctx->size), 1, ctx->file);
    fseek(ctx->file, 0, SEEK_SET);

    u32 magic = read32((u32) buffer);
    if(magic != ANCAST_MAGIC) {
        printf("ancast: %s is not an ancast image (magic is 0x%08lX, expected 0x%08lX).\n", path, magic, ANCAST_MAGIC);
        return -2;
    }

    u32 sig_offset = read32((u32) &buffer[0x08]);
    u32 sig_type = read32((u32) &buffer[sig_offset]);

    u32 header_offset = 0;
    switch(sig_type) {
        case 0x01:
            header_offset = 0xA0;
            break;
        case 0x02:
            header_offset = 0x1A0;
            break;
        default:
            printf("ancast: %s has unrecognized signature type 0x%02lX.\n", path, sig_type);
            return -3;
    }

    ctx->header_size = header_offset + sizeof(ancast_header);
    memcpy(&ctx->header, &buffer[header_offset], sizeof(ancast_header));

    return 0;
}

int ancast_init_from_raw_sector(ancast_ctx* ctx, int sector_idx)
{
    if(!ctx) return -1;
    memset(ctx, 0, sizeof(ancast_ctx));

    ctx->path = "";
    ctx->file = 0;
    ctx->size = 0x80000;
    ctx->sector_idx = sector_idx;

    sdcard_ack_card();
    sdcard_read(sector_idx, 1, sd_read_buffer);

    u32 magic = read32((u32) sd_read_buffer);
#ifdef MINUTE_BOOT1
    serial_send_u32(magic);
#endif
    if(magic != ANCAST_MAGIC) {
        printf("SD ancast is not an ancast image (magic is 0x%08lX, expected 0x%08lX).\n", magic, ANCAST_MAGIC);
        return -2;
    }

    u32 sig_offset = read32((u32) &sd_read_buffer[0x08]);
    u32 sig_type = read32((u32) &sd_read_buffer[sig_offset]);

    u32 header_offset = 0;
    switch(sig_type) {
        case 0x01:
            header_offset = 0xA0;
            break;
        case 0x02:
            header_offset = 0x1A0;
            break;
        default:
            printf("SD ancast has unrecognized signature type 0x%02lX.\n", sig_type);
            return -3;
    }

    ctx->header_size = header_offset + sizeof(ancast_header);
    memcpy(&ctx->header, &sd_read_buffer[header_offset], sizeof(ancast_header));

    return 0;
}

int ancast_init_from_memory(ancast_ctx* ctx, void* ancast_mem)
{
    if(!ctx) return -1;
    memset(ctx, 0, sizeof(ancast_ctx));

    ctx->path = "";
    ctx->file = 0;
    ctx->size = 0x80000;
    ctx->memory_load = ancast_mem;


    u32 magic = read32((u32) ancast_mem);
#ifdef MINUTE_BOOT1
    serial_send_u32(magic);
#endif
    if(magic != ANCAST_MAGIC) {
        printf("SD ancast is not an ancast image (magic is 0x%08lX, expected 0x%08lX).\n", magic, ANCAST_MAGIC);
        return -2;
    }

    u32 sig_offset = read32(((u32)ancast_mem) + 8);
    u32 sig_type = read32(((u32)ancast_mem) + sig_offset);

    u32 header_offset = 0;
    switch(sig_type) {
        case 0x01:
            header_offset = 0xA0;
            break;
        case 0x02:
            header_offset = 0x1A0;
            break;
        default:
            printf("SD ancast has unrecognized signature type 0x%02lX.\n", sig_type);
            return -3;
    }

    ctx->header_size = header_offset + sizeof(ancast_header);
    memcpy(&ctx->header, (void*)(((u32)ancast_mem) + header_offset), sizeof(ancast_header));

    return 0;
}

int ancast_load(ancast_ctx* ctx)
{
    if(!ctx) return -1;

    u8 target = ctx->header.device >> 4;
    u8 type = ctx->header.device & 0x0F;

    switch(target) {
        case ANCAST_TARGET_PPC:
            switch(type) {
                case 0x01:
                    ctx->load = (void*)0x08000000;
                    break;

                case 0x03:
                    ctx->load = (void*)0x01330000;
                    break;
            }
            break;

        case ANCAST_TARGET_IOP:
            ctx->load = (void*)0x01000000;
            break;

        default: break;
    }

    if(!ctx->load) {
        printf("ancast: unknown load address for %s (device 0x%02lX).\n", ctx->path, ctx->header.device);
        ancast_fini(ctx);
        return -2;
    }

    ctx->body = ctx->load + ctx->header_size;

    if (ctx->memory_load)
    {
        u32 total_size = ctx->header_size + ctx->header.body_size;
        memcpy(ctx->load, ctx->memory_load, total_size);
    }
#ifndef MINUTE_BOOT1
    else if (ctx->file)
    {
        printf("ancast: reading 0x%x bytes from %s\n", ctx->header_size + ctx->header.body_size, ctx->path);
        fseek(ctx->file, 0, SEEK_SET);

        u32 total_size = ctx->header_size + ctx->header.body_size;

#ifdef MINUTE_BOOT1
        serial_send_u32(total_size);
        serial_send_u32(ctx->header.body_size);
#endif

#if 1
        int led_alternate = 0;
        for (u32 i = 0; i < total_size; i += 0x100000)
        {
            if (i % 0x100000 == 0)
            {
                printf("ancast: ...%08x -> %08x\n", i, (u32)ctx->load + i);
                if (led_alternate) {
                    smc_set_notification_led(LEDRAW_BLUE);
                }
                else {
                    smc_set_notification_led(LEDRAW_PURPLE);
                }
                led_alternate = !led_alternate;
            }

            u32 to_read = 0x100000;
            if (i + to_read > total_size) {
                to_read = total_size - i;
            }

            int count = fread(ctx->load + i, to_read, 1, ctx->file);
            if(count != 1) {
                printf("ancast: failed to read offs=%08x, %s (%d).\n", i, ctx->path, errno);
                ancast_fini(ctx);
                return errno;
            }
        }
#endif

#if 0
        int count = fread(ctx->load, total_size, 1, ctx->file);
        if(count != 1) {
            printf("ancast: failed to read offs=%08x, %s (%d).\n", 0, ctx->path, errno);
            ancast_fini(ctx);
            return errno;
        }
#endif
        smc_set_notification_led(LEDRAW_PURPLE);
        
        printf("ancast: done reading\n");
    }
#endif
    else if (ctx->sector_idx)
    {
        void* sdcard_dst = ctx->load;
        u32 num_sectors = ctx->header_size + ctx->header.body_size;
        num_sectors /= 0x200;
        num_sectors += 1; // sloppy, but not the end of the world.

#ifdef MINUTE_BOOT1
        serial_send_u32(num_sectors);
        serial_send_u32(ctx->header.body_size);
#endif

        memset(sdcard_dst, 0, ctx->header_size + ctx->header.body_size);

        int led_alternate = 0;
        for (int i = 0; i < num_sectors; i++)
        {
            sdcard_dst = (void*)((u32)ctx->load + (i*0x200));
            //serial_send_u32(i);

#ifdef MINUTE_BOOT1
            if (i % 0x10 == 0) {
                serial_send_u32(i);
                if (led_alternate) {
                    smc_set_notification_led(LEDRAW_BLUE);
                }
                else {
                    smc_set_notification_led(LEDRAW_PURPLE);
                }
                led_alternate = !led_alternate;
            }
#endif
            //serial_send_u32((u32)sdcard_dst);
            sdcard_read(ctx->sector_idx + i, 1, sdcard_dst);
            //serial_send_u32(*(u32*)sdcard_dst);
            //sdcard_read(ctx->sector_idx + i, 1, sdcard_dst);
            //serial_send_u32(*(u32*)sdcard_dst);
            // TODO: why???
        }
#ifdef MINUTE_BOOT1
        smc_set_notification_led(LEDRAW_PURPLE);
#endif
    }

#ifndef MINUTE_BOOT1
    u32 hash[SHA_HASH_WORDS] = {0};
    sha_hash(ctx->body, hash, ctx->header.body_size);

    u32* h1 = ctx->header.body_hash;
    u32* h2 = hash;
    if(memcmp(h1, h2, SHA_HASH_SIZE) != 0) {
        printf("ancast: body hash check failed.\n");
        printf("        expected:   %08lX%08lX%08lX%08lX%08lX\n", h1[0], h1[1], h1[2], h1[3], h1[4]);
        printf("        calculated: %08lX%08lX%08lX%08lX%08lX\n", h2[0], h2[1], h2[2], h2[3], h2[4]);

        FILE* f_test = fopen("sdmc:/test_sha.bin", "wb");
        if(!f_test)
        {
            printf("ancast: failed to open file.\n");
            return -3;
        }
        
        fwrite((void*)ctx->body, 1, ctx->header.body_size, f_test);
        fclose(f_test);

        return -3;
    }
#endif

    return 0;
}

int ancast_fini(ancast_ctx* ctx)
{
#ifndef MINUTE_BOOT1
    if (ctx->file)
    {
        int res = fclose(ctx->file);
        if(res) {
            printf("ancast: failed to close %s (%d).\n", ctx->path, res);
            return res;
        }
    }
#endif

    printf("ancast: fini\n");

    memset(ctx, 0, sizeof(ancast_ctx));
    return 0;
}

typedef struct {
    u32 header_size;
    u32 loader_size;
    u32 elf_size;
    u32 ddr_init;
} ios_header;

u32 ancast_iop_load(const char* path)
{
    int res = 0;
    ancast_ctx ctx = {0};

    res = ancast_init(&ctx, path);
    if(res) return 0;

    u8 target = ctx.header.device >> 4;
    if(target != ANCAST_TARGET_IOP) {
        printf("ancast: %s is not an IOP image (target is 0x%02X, expected 0x%02X).\n", path, target, ANCAST_TARGET_IOP);
        ancast_fini(&ctx);
        return 0;
    }

    res = ancast_load(&ctx);
    if(res) return 0;

#ifndef MINUTE_BOOT1
    if(!(ctx.header.unk1 & 0b1)) {
        aes_reset();
        aes_set_key(otp.fw_ancast_key);

        static const u8 iv[16] = {0x91, 0xC9, 0xD0, 0x08, 0x31, 0x28, 0x51, 0xEF,
                                  0x6B, 0x22, 0x8B, 0xF1, 0x4B, 0xAD, 0x43, 0x22};
        aes_set_iv((u8*)iv);

        printf("ancast: decrypting %s...\n", path);
        aes_decrypt(ctx.body, ctx.body, ctx.header.body_size / 0x10, 0);
    }
#endif

    dc_flushrange(ctx.load, ctx.header_size + ctx.header.body_size);

    ios_header* header = ctx.body;
    u32 vector = (u32) ctx.body + header->header_size;

    res = ancast_fini(&ctx);
    if(res) return 0;

    return vector;
}

u32 ancast_ppc_load(const char* path)
{
    int res = 0;
    ancast_ctx ctx = {0};

    res = ancast_init(&ctx, path);
    if(res) return 0;

    u8 target = ctx.header.device >> 4;
    if(target != ANCAST_TARGET_PPC) {
        printf("ancast: %s is not a PPC image (target is 0x%02X, expected 0x%02X).\n", path, target, ANCAST_TARGET_PPC);
        ancast_fini(&ctx);
        return 0;
    }

    res = ancast_load(&ctx);
    if(res) return 0;

    dc_flushrange(ctx.load, ctx.header_size + ctx.header.body_size);
    u32 vector = (u32) ctx.body;

    res = ancast_fini(&ctx);
    if(res) return 0;

    return vector;
}

u32 ancast_iop_load_from_raw_sector(int sector_idx)
{
    int res = 0;
    ancast_ctx ctx = {0};

    res = ancast_init_from_raw_sector(&ctx, sector_idx);
    if(res) return 0;

    u8 target = ctx.header.device >> 4;
    if(target != ANCAST_TARGET_IOP) {
        printf("SD ancast is not an IOP image (target is 0x%02X, expected 0x%02X).\n", target, ANCAST_TARGET_IOP);
        ancast_fini(&ctx);
        return 0;
    }

    res = ancast_load(&ctx);
    if(res) return 0;

#ifndef MINUTE_BOOT1
    if(!(ctx.header.unk1 & 0b1)) {
        aes_reset();
        aes_set_key(otp.fw_ancast_key);

        static const u8 iv[16] = {0x91, 0xC9, 0xD0, 0x08, 0x31, 0x28, 0x51, 0xEF,
                                  0x6B, 0x22, 0x8B, 0xF1, 0x4B, 0xAD, 0x43, 0x22};
        aes_set_iv((u8*)iv);

        printf("ancast: decrypting...\n");
        aes_decrypt(ctx.body, ctx.body, ctx.header.body_size / 0x10, 0);
    }
#endif

    ios_header* header = ctx.body;
    u32 vector = (u32) ctx.body + header->header_size;

#ifdef MINUTE_BOOT1
    serial_send_u32((u32) ctx.body);
    serial_send_u32(header->header_size);
    serial_send_u32(vector);
#endif

    res = ancast_fini(&ctx);
    if(res) return 0;

    return vector;
}

u32 ancast_iop_load_from_memory(void* ancast_mem)
{
    int res = 0;
    ancast_ctx ctx = {0};

    res = ancast_init_from_memory(&ctx, ancast_mem);
    if(res) return 0;

    u8 target = ctx.header.device >> 4;
    if(target != ANCAST_TARGET_IOP) {
        printf("SD ancast is not an IOP image (target is 0x%02X, expected 0x%02X).\n", target, ANCAST_TARGET_IOP);
        ancast_fini(&ctx);
        return 0;
    }

    res = ancast_load(&ctx);
    if(res) return 0;

#ifndef MINUTE_BOOT1
    if(!(ctx.header.unk1 & 0b1)) {
        aes_reset();
        aes_set_key(otp.fw_ancast_key);

        static const u8 iv[16] = {0x91, 0xC9, 0xD0, 0x08, 0x31, 0x28, 0x51, 0xEF,
                                  0x6B, 0x22, 0x8B, 0xF1, 0x4B, 0xAD, 0x43, 0x22};
        aes_set_iv((u8*)iv);

        printf("ancast: decrypting...\n");
        aes_decrypt(ctx.body, ctx.body, ctx.header.body_size / 0x10, 0);
    }
#endif

    ios_header* header = ctx.body;
    u32 vector = (u32) ctx.body + header->header_size;

#ifdef MINUTE_BOOT1
    serial_send_u32((u32) ctx.body);
    serial_send_u32(header->header_size);
    serial_send_u32(vector);
#endif

    res = ancast_fini(&ctx);
    if(res) return 0;

    return vector;
}

u32 ancast_patch_load(const char* fn_ios, const char* fn_patch)
{
    u32* patch_base = (u32*)0x100;
    FILE* f_patch = fopen(fn_patch, "rb");
    if(!f_patch)
    {
        printf("ancast: failed to open patch file %s .\n", fn_patch);
        return 0;
    }
    
    fread((void*)patch_base, 1, ALL_PURPOSE_TMP_BUF-0x100, f_patch);
    fclose(f_patch);
    
    // sanity-check our patches
    if(memcmp(patch_base, "SALTPTCH", 8))
    {
        printf("ancast: invalid patch magic!\n");
        return 0;
    }
    if(patch_base[2] != 1)
    {
        printf("ancast: unknown version 0x%X\n", (unsigned int)patch_base[2]);
        return 0;
    }
    
    // load IOS image
    u32 vector = ancast_iop_load(fn_ios);
    if(vector == 0)
        return 0;
    
    // check to be sure IOS image is 5.5.0 (todo: move this to patches somehow?)
    u32 hash[SHA_HASH_WORDS] = {0};
    u8 expected_hash[20] = {0x12, 0x2D, 0x17, 0x82, 0x32, 0x5C, 0x73, 0x0F, 0x0A, 
    0x5D, 0x25, 0xEA, 0xE4, 0x91, 0xFA, 0xB4, 0xEC, 0xF2, 0x90, 0x37};
    sha_hash((void*)0x01000000, hash, 0x200);
    if(memcmp(hash, expected_hash, sizeof(hash)))
    {
        printf("ancast: IOS image might not be 5.5!\n");
        //return 0;
    }
    
    // find elfldr's jumpout addr and patch it
    u32 hook_base = 0;
    for(u32 i = 0; i < 0x1000; i += 4)
    {
        if(*(u32*)(vector + i) == 0xFFFF0000)
        {
            hook_base = vector + i;
            break;
        }
    }
    if(hook_base == 0)
    {
        printf("ancast: failed to find elfloader jumpout magic!\n");
        return 0;
    }

    *(u32*)hook_base = ALL_PURPOSE_TMP_BUF;
    // copy code out
    memcpy((void*)ALL_PURPOSE_TMP_BUF, elfldr_patch, sizeof(elfldr_patch));
    
    return vector;
}