/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */
 
 #include "prsh.h"

#include "types.h"
#include "utils.h"
#include <string.h>
#include "gfx.h"
#include "crypto.h"

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
    u32 field_14;
    u32 field_18;
    u32 field_1C;

    u32 field_20;
    u32 field_24;
    u32 field_28;
    u32 field_2C;

    u32 field_30;
    u32 field_34;
    u32 boot1_main;
    u32 boot1_read;

    u32 boot1_verify;
    u32 boot1_decrypt;
    u32 boot0_main;
    u32 boot0_read;

    u32 boot0_verify;
    u32 boot0_decrypt;
} PACKED boot_info_t;

static prst_entry* prst = NULL;
static prsh_header* header = NULL;
static bool initialized = false;
extern otp_t otp;

void prsh_reset(void)
{
    prst = NULL;
    header = NULL;
    initialized = false;
}

void prsh_init(void)
{
    // corrupt
    if (header && header->total_entries > 0x100) {
        printf("prsh: Detected corruption, %u total entries.\nprsh: Reinitializing.\n", header->total_entries);
        initialized = false;
    }

    if(initialized) return;

    void* buffer = (void*)0x10000400;
    size_t size = 0x7C00;
    while(size) {
        if(!memcmp(buffer, "PRSH", sizeof(u32))) break;
        buffer += sizeof(u32);
        size -= sizeof(u32);

        //printf("%08x: %08x\n", buffer, *(u32*)buffer);
    }

    if (!size) {
        // clear bad PRSH data
        memset((u8 *)0x10000400, 0,0x7C00);

        u32 total_entries = 0x20;

        /* create PRSH */
        header = (prsh_header*)0x10005A54;
        header->magic = 0x50525348; // "PRSH"
        header->version = 1;
        header->is_boot1 = 1;
        header->total_entries = total_entries;
        header->entries = 0x1;
        header->size = sizeof(*header);
        header->size += total_entries * sizeof(prsh_entry);

        memset((u8 *)&header->entry[0], 0, total_entries * sizeof(prsh_entry));

        prst = (prst_entry*)&header->entry[total_entries];
        prst->size = header->size;
        prst->is_set = 1;
        prst->magic= 0x50525354; // "PRST"

        // create boot_info
        prsh_entry* boot_info_ent = &header->entry[0];
        strncpy(boot_info_ent->name, "boot_info", 0x100);
        boot_info_ent->data = (void *)0x10008000;
        boot_info_ent->size = 0x58;
        boot_info_ent->is_set = 0x80000000;

        boot_info_t* boot_info = (boot_info_t*)0x10008000;
        boot_info->is_coldboot = 1;
        boot_info->boot_flags = 0x04000080;
        boot_info->boot_state = 0;
        boot_info->boot_count = 1;
        boot_info->field_10 = 0;
        boot_info->field_14 = 0;
        boot_info->field_18 = 0xFFFFFFFF;
        boot_info->field_1C = 0xFFFFFFFF;
        boot_info->field_20 = 0xFFFFFFFF;
        boot_info->field_24 = 0xFFFFFFFF;
        boot_info->field_28 = 0xFFFFFFFF;
        boot_info->field_2C = 0xFFFFFFFF;
        boot_info->field_30 = 0;
        boot_info->field_34 = 0;

        // Dummy values
        boot_info->boot1_main = 0x00369F6B;
        boot_info->boot1_read = 0x00297268;
        boot_info->boot1_verify = 0x0005FCFE;
        boot_info->boot1_decrypt = 0x00053CE8;
        boot_info->boot0_main = 0x00012030;
        boot_info->boot0_read = 0x000029D2;
        boot_info->boot0_verify = 0x0000D281;
        boot_info->boot0_decrypt = 0x0000027A;

        // TODO: we could pass in a ram-only OTP here, maybe?
        printf("prsh: No header found, made a new one.\n");
    }
    else {
        header = buffer - sizeof(u32);
        prst = (prst_entry*)&header->entry[header->total_entries];
    }

#ifndef MINUTE_BOOT1
    printf("prsh: Header at %08x, PRST at %08x, %u entries (%u capacity):\n", header, prst, header->entries, header->total_entries);
    /*for (int i = 0; i < header->entries; i++) {
        printf("    %u: %s\n", i, header->entry[i].name);
    }*/
#endif

    // TODO: verify checksums
    // TODO: increment counts as boot1
    
    initialized = true;
}

int prsh_get_entry(const char* name, void** data, size_t* size)
{
    prsh_init();
    if(!name) return -1;
    if (header->total_entries > 0x100) return -1; // corrupt 

    for(int i = 0; i < header->entries; i++) {
        prsh_entry* entry = &header->entry[i];

        if(!strncmp(name, entry->name, sizeof(entry->name))) {
            if(data) *data = entry->data;
            if(size) *size = entry->size;
            return 0;
        }
    }

    return -2;
}

int prsh_set_entry(const char* name, void* data, size_t size)
{
    prsh_init();
    if(!name) return -1;
    if (header->total_entries > 0x100) return -1; // corrupt

    for(int i = 0; i < header->entries; i++) {
        prsh_entry* entry = &header->entry[i];

        if(!strncmp(name, entry->name, sizeof(entry->name))) {
            entry->data = data;
            entry->size = size;
            entry->is_set = 0x80000000;
            prsh_recompute_checksum();
            return 0;
        }
    }

    return -2;
}

void prsh_recompute_checksum()
{
    prsh_init();
    // Re-calculate PRSH checksum
    u32 checksum = 0;
    u32 word_counter = 0;
    
    void* checksum_start = (void*)(&header->magic);
    while (word_counter < ((header->entries * sizeof(prsh_entry))>>2))
    {
        checksum ^= *(u32 *)(checksum_start + word_counter * 0x04);
        word_counter++;
    }
    
    printf("%08x %08x\n", header->checksum, checksum);
    
    header->checksum = checksum;

    checksum = 0;
    word_counter = 1;
    void* prst_checksum_start = (void*)(&prst->size);
    while (word_counter < (sizeof(prst_entry)/sizeof(u32)))
    {
        checksum ^= *(u32 *)(prst_checksum_start + word_counter * 0x04);
        word_counter++;
    }
    
    printf("%08x %08x\n", header->checksum, checksum);
    
    prst->checksum = checksum;
}

void prsh_decrypt()
{
    if (!(otp.security_level & 0x80000000)) {
        return;
    }

    aes_reset();
    aes_set_key(otp.fw_ancast_key);

    static const u8 iv[16] = {0x0A, 0xAB, 0xA5, 0x30, 0x2E, 0x90, 0x12, 0xD9, 
                              0x08, 0x51, 0x74, 0xE8, 0x6B, 0x83, 0xEC, 0x22};

    aes_set_iv((u8*)iv);
    aes_decrypt((void*)0x10000400, (void*)0x10000400, 0x7C00 / 0x10, 0);
}


void prsh_encrypt()
{
    if (!(otp.security_level & 0x80000000)) {
        return;
    }

    aes_reset();
    aes_set_key(otp.fw_ancast_key);

    static const u8 iv[16] = {0x0A, 0xAB, 0xA5, 0x30, 0x2E, 0x90, 0x12, 0xD9, 
                              0x08, 0x51, 0x74, 0xE8, 0x6B, 0x83, 0xEC, 0x22};

    aes_set_iv((u8*)iv);
    aes_encrypt((void*)0x10000400, (void*)0x10000400, 0x7C00 / 0x10, 0);
}
