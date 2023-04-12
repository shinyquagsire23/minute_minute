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

#include "sdmmc.h"
#include "sdhc.h"
#include "sdcard.h"
#include "mlc.h"
#include "nand.h"

#include "ff.h"

#include "smc.h"
#include "crypto.h"

#ifndef MINUTE_BOOT1

// TODO: how many sectors is 8gb MLC WFS?
#define TOTAL_SECTORS (0x3A20000)

extern seeprom_t seeprom;
extern otp_t otp;

void dump_factory_log()
{
    FILE* f_log = NULL;
    int ret = 0;

    gfx_clear(GFX_ALL, BLACK);

    f_log = fopen("sdmc:/factory-log.txt", "wb");
    if(!f_log)
    {
        printf("Failed to open sdmc:/factory-log.txt\n");
        goto close_ret;
    }
    u8* sector_buf = memalign(32, SDMMC_DEFAULT_BLOCKLEN * SDHC_BLOCK_COUNT_MAX);

    // calculate number of extra sectors
    u32 total_sec = mlc_get_sectors();

    u32 block_size_bytes = SDHC_BLOCK_COUNT_MAX * 0x200;
    for(u32 sector = TOTAL_SECTORS; sector < total_sec; sector += SDHC_BLOCK_COUNT_MAX)
    {
        do ret = mlc_read(sector, SDHC_BLOCK_COUNT_MAX, sector_buf);
        while(ret);

        // stop dumping at the first 0x00 byte
        int i;
        for(i = 0; i < block_size_bytes; i++)
        {
            if(sector_buf[i] != 0)
                continue;
            fwrite(sector_buf, 1, i, f_log);
            break;
        }
        // break when we've written the end of the log
        if(i != block_size_bytes) break;

        fwrite(sector_buf, 1, block_size_bytes, f_log);
    }

    free(sector_buf);
    printf("\nDone!\n");

close_ret:
    if(f_log) fclose(f_log);

    printf("Press POWER or EJECT to return...\n");
    smc_wait_events(SMC_POWER_BUTTON | SMC_EJECT_BUTTON);
}

void dump_seeprom_otp()
{
    gfx_clear(GFX_ALL, BLACK);

    FILE* f_otp = fopen("sdmc:/otp.bin", "wb");
    if(!f_otp)
    {
        printf("Failed to open sdmc:/otp.bin.\n");
        goto ret;
    }
    fwrite(&otp, 1, sizeof(otp_t), f_otp);
    fclose(f_otp);

    FILE* f_eep = fopen("sdmc:/seeprom.bin", "wb");
    if(!f_eep)
    {
        printf("Failed to open sdmc:/seeprom.bin.\n");
        goto ret;
    }
    fwrite(&seeprom, 1, sizeof(seeprom_t), f_eep);
    fclose(f_eep);

    printf("\nDone!\n");
ret:
    printf("Press POWER or EJECT to return...\n");
    smc_wait_events(SMC_POWER_BUTTON | SMC_EJECT_BUTTON);
}

int _dump_mlc(u32 base)
{
    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    int res = 0, mres = 0, sres = 0;
    if(base == 0) return -2;

    // This uses "async" read/write functions, combined with double buffering to achieve a
    // much faster dump. This works because these are two separate host controllers using DMA.
    // Instead of running a single command and waiting for completion, we queue both commands
    // and then wait for them both to complete at the end of each iteration.
    struct sdmmc_command mlc_cmd = {0}, sdcard_cmd = {0};

    u8* sector_buf1 = memalign(32, SDMMC_DEFAULT_BLOCKLEN * SDHC_BLOCK_COUNT_MAX);
    u8* sector_buf2 = memalign(32, SDMMC_DEFAULT_BLOCKLEN * SDHC_BLOCK_COUNT_MAX);

    u8* mlc_buf = sector_buf2;
    u8* sdcard_buf = sector_buf1;

    // Fill one of the buffers in advance, so SD card has something to work with.
    do res = mlc_read(0, SDHC_BLOCK_COUNT_MAX, sdcard_buf);
    while(res);

    // Do one less iteration than we need, due to having to special case the start and end.
    u32 sdcard_sector = base;
    for(u32 sector = 0; sector < (TOTAL_SECTORS - SDHC_BLOCK_COUNT_MAX); sector += SDHC_BLOCK_COUNT_MAX)
    {
        int complete = 0;
        // Make sure to retry until the command succeeded, probably superfluous but harmless...
        while(complete != 0b11) {
            // Issue commands if we didn't already complete them.
            if(!(complete & 0b01))
                mres = mlc_start_read(sector + SDHC_BLOCK_COUNT_MAX, SDHC_BLOCK_COUNT_MAX, mlc_buf, &mlc_cmd);
            if(!(complete & 0b10))
                sres = sdcard_start_write(sdcard_sector, SDHC_BLOCK_COUNT_MAX, sdcard_buf, &sdcard_cmd);

            // Only end the command if starting it succeeded.
            // If starting and ending the command succeeds, mark it as complete.
            if(!(complete & 0b01) && mres == 0) {
                mres = mlc_end_read(&mlc_cmd);
                if(mres == 0) complete |= 0b01;
            }
            if(!(complete & 0b10) && sres == 0) {
                sres = sdcard_end_write(&sdcard_cmd);
                if(sres == 0) complete |= 0b10;
            }
        }

        // Swap buffers.
        if(mlc_buf == sector_buf1) {
            mlc_buf = sector_buf2;
            sdcard_buf = sector_buf1;
        } else {
            mlc_buf = sector_buf1;
            sdcard_buf = sector_buf2;
        }

        sdcard_sector += SDHC_BLOCK_COUNT_MAX;

        if((sector % 0x10000) == 0) {
            printf("MLC: Sector 0x%08lX completed\n", sector);
        }
    }

    // Finish up the last iteration.
    do res = sdcard_write(sdcard_sector, SDHC_BLOCK_COUNT_MAX, sdcard_buf);
    while(res);

    free(sector_buf1);
    free(sector_buf2);

    return 0;
}

int _dump_slc_raw(u32 bank)
{
    #define PAGES_PER_ITERATION (0x10)
    #define TOTAL_ITERATIONS (NAND_MAX_PAGE / PAGES_PER_ITERATION)

    static u8 page_buf[PAGE_SIZE] ALIGNED(64);
    static u8 ecc_buf[ECC_BUFFER_ALLOC] ALIGNED(128);

    static u8 file_buf[PAGES_PER_ITERATION][PAGE_SIZE + PAGE_SPARE_SIZE];

    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    const char* name = NULL;
    switch(bank) {
        case NAND_BANK_SLC: name = "SLC"; break;
        case NAND_BANK_SLCCMPT: name = "SLCCMPT"; break;
        default: return -2;
    }

    char path[64] = {0};
    sprintf(path, "%s.RAW", name);

    FIL file = {0}; FRESULT fres = 0; UINT btx = 0;
    fres = f_open(&file, path, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if(fres != FR_OK) {
        printf("Failed to open %s (%d).\n", path, fres);
        return -3;
    }

    printf("Initializing %s...\n", name);
    nand_initialize(bank);

    for(u32 i = 0; i < TOTAL_ITERATIONS; i++)
    {
        u32 page_base = i * PAGES_PER_ITERATION;
        for(u32 page = 0; page < PAGES_PER_ITERATION; page++)
        {
            nand_read_page(page_base + page, page_buf, ecc_buf);
            nand_wait();
            nand_correct(page_base + page, page_buf, ecc_buf);

            memcpy(file_buf[page], page_buf, PAGE_SIZE);
            memcpy(file_buf[page] + PAGE_SIZE, ecc_buf, PAGE_SPARE_SIZE);
        }

        fres = f_write(&file, file_buf, sizeof(file_buf), &btx);
        if(fres != FR_OK || btx != sizeof(file_buf)) {
            f_close(&file);
            printf("Failed to write %s (%d).\n", path, fres);
            return -4;
        }

        if((i % 0x100) == 0) {
            printf("%s-RAW: Page 0x%05lX completed\n", name, page_base);
        }
    }

    fres = f_close(&file);
    if(fres != FR_OK) {
        printf("Failed to close %s (%d).\n", path, fres);
        return -5;
    }

    return 0;

    #undef PAGES_PER_ITERATION
    #undef TOTAL_ITERATIONS
}

int _dump_slc(u32 base, u32 bank)
{
    // how many sectors needed for a page (4)
    #define SECTORS_PER_PAGE (PAGE_SIZE / SDMMC_DEFAULT_BLOCKLEN)
    // the SD host controller can only transfer 512 sectors at a time (512*512 bytes)
    #define SECTORS_PER_ITERATION (SDHC_BLOCK_COUNT_MAX)
    // given the above constraint, the max number of SLC pages we can transfer to SD at a time (128)
    #define PAGES_PER_ITERATION (SECTORS_PER_ITERATION / SECTORS_PER_PAGE)
    // the number of SD transfer iterations required to complete the SLC dump (0x800)
    #define TOTAL_ITERATIONS (NAND_MAX_PAGE / PAGES_PER_ITERATION)

    static u8 page_buf[PAGES_PER_ITERATION][PAGE_SIZE] ALIGNED(64);
    static u8 ecc_buf[ECC_BUFFER_ALLOC] ALIGNED(128);

    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    int res = 0;
    if(base == 0) return -2;

    const char* name = NULL;
    switch(bank) {
        case NAND_BANK_SLC: name = "SLC"; break;
        case NAND_BANK_SLCCMPT: name = "SLCCMPT"; break;
        default: return -3;
    }

    printf("Initializing %s...\n", name);
    nand_initialize(bank);

    u32 sdcard_sector = base;
    for(u32 i = 0; i < TOTAL_ITERATIONS; i++)
    {
        u32 page_base = i * PAGES_PER_ITERATION;
        for(u32 page = 0; page < PAGES_PER_ITERATION; page++)
        {
            nand_read_page(page_base + page, page_buf[page], ecc_buf);
            nand_wait();
            nand_correct(page_base + page, page_buf[page], ecc_buf);
        }

        do res = sdcard_write(sdcard_sector, SECTORS_PER_ITERATION, page_buf);
        while(res);

        sdcard_sector += SECTORS_PER_ITERATION;

        if((i % 0x100) == 0) {
            printf("%s: Page 0x%05lX completed\n", name, page_base);
        }
    }

    return 0;

    #undef SECTORS_PER_PAGE
    #undef SECTORS_PER_ITERATION
    #undef PAGES_PER_ITERATION
    #undef TOTAL_ITERATIONS
}

int _dump_copy_rednand(u32 slc_base, u32 slccmpt_base, u32 mlc_base)
{
    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    if(slc_base == 0 && slccmpt_base == 0 && mlc_base == 0) {
        return -2;
    }

    // Dump SLC.
    if(slc_base != 0) {
        _dump_slc(slc_base, NAND_BANK_SLC);
    }

    // Dump SLCCMPT.
    if(slccmpt_base != 0) {
        _dump_slc(slccmpt_base, NAND_BANK_SLCCMPT);
    }

    // Dump MLC.
    if(mlc_base != 0) {
        _dump_mlc(mlc_base);
    }

    return 0;
}

int _dump_partition_rednand(void)
{
    int res = 0;
    FRESULT fres = 0;

    u8 mbr[SDMMC_DEFAULT_BLOCKLEN] ALIGNED(32) = {0};
    u8* table = &mbr[0x1BE];
    u8* part2 = &table[0x10];
    u8* part3 = &table[0x20];
    u8* part4 = &table[0x30];

    res = sdcard_read(0, 1, mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        return -1;
    }

    // Already partitioned, so ask about repartitioning
    if(part2[0x4] == 0xAE && part3[0x4] == 0xAE && part4[0x4] == 0xAE)
    {
        printf("Repartition SD card?\n");
        printf("[POWER] No | [EJECT] Yes...\n");
        u8 input = smc_wait_events(SMC_POWER_BUTTON | SMC_EJECT_BUTTON);
        if(input & SMC_POWER_BUTTON) return 0;
    }

    printf("Partitioning SD card...\n");

    const u32 slc_sectors = (NAND_MAX_PAGE * PAGE_SIZE) / SDMMC_DEFAULT_BLOCKLEN;
    const u32 mlc_sectors = 0x03A20000; // TODO: 8GB model.
    const u32 data_sectors = 0x100000 / SDMMC_DEFAULT_BLOCKLEN;

    u32 end = (u32)sdcard_get_sectors() & 0xFFFF0000;
    u32 slccmpt_base = end - slc_sectors;
    u32 slc_base = slccmpt_base - slc_sectors;
    u32 mlc_base = slc_base - mlc_sectors;
    u32 data_base = mlc_base - data_sectors;
    u32 start = data_base;

    u32 fat_base = 0;
    u32 fat_size = start - 1;

    printf("Partition layout on SD with 0x%08lX (0x%08lX) sectors:\n", (u32)sdcard_get_sectors(), end);

    printf("FAT32:   0x%08lX->0x%08lX\n", fat_base, fat_base + fat_size);
    printf("DATA:    0x%08lX->0x%08lX\n", data_base, data_base + data_sectors);
    printf("MLC:     0x%08lX->0x%08lX\n", mlc_base, mlc_base + mlc_sectors);
    printf("SLC:     0x%08lX->0x%08lX\n", slc_base, slc_base + slc_sectors);
    printf("SLCCMPT: 0x%08lX->0x%08lX\n", slccmpt_base, slccmpt_base + slc_sectors);

    printf("[POWER] Exit | [EJECT] Continue...\n");
    u8 input = smc_wait_events(SMC_POWER_BUTTON | SMC_EJECT_BUTTON);
    if(input & SMC_POWER_BUTTON) return 1;

    printf("Formatting to FAT32...\n");
    fres = f_mkfs("sdmc:", 0, 0, fat_base, fat_base + fat_size);
    if(fres != FR_OK) {
        printf("Failed to format card (%d)!\n", fres);
        return -2;
    }

    printf("Updating MBR...\n");

    res = sdcard_read(0, 1, mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        return -3;
    }

    memset(part2, 0x00, 0x10);
    part2[0x4] = 0xAE;
    ST_DWORD(&part2[0x8], data_base);
    ST_DWORD(&part2[0xC], data_sectors);

    memset(part3, 0x00, 0x10);
    part3[0x4] = 0xAE;
    ST_DWORD(&part3[0x8], mlc_base);
    ST_DWORD(&part3[0xC], mlc_sectors);

    memset(part4, 0x00, 0x10);
    part4[0x4] = 0xAE;
    ST_DWORD(&part4[0x8], slc_base);
    ST_DWORD(&part4[0xC], slc_sectors * 2);

    res = sdcard_write(0, 1, mbr);
    if(res) {
        printf("Failed to write MBR (%d)!\n", res);
        return -4;
    }

    return 0;
}

void dump_slc(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Dumping SLC...\n");

    u8 mbr[SDMMC_DEFAULT_BLOCKLEN] ALIGNED(32) = {0};
    u8* table = &mbr[0x1BE];
    u8* part4 = &table[0x30];

    res = sdcard_read(0, 1, mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        goto slc_exit;
    }

    u32 slc_base = LD_DWORD(&part4[0x8]);

    res = _dump_slc(slc_base, NAND_BANK_SLC);
    if(res) {
        printf("Failed to dump SLC (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void dump_format_rednand(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Formatting redNAND...\n");

    u8 mbr[SDMMC_DEFAULT_BLOCKLEN] ALIGNED(32) = {0};
    u8* table = &mbr[0x1BE];
    u8* part3 = &table[0x20];
    u8* part4 = &table[0x30];

    res = _dump_partition_rednand();
    if(res > 0) return;
    if(res < 0) goto format_exit;

    res = sdcard_read(0, 1, mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        goto format_exit;
    }

    printf("Dump SLC/SLCCMPT-RAW images? These are useful for sysNAND restore.\n");
    printf("[POWER] Skip | [EJECT] Dump...\n");
    u8 input = smc_wait_events(SMC_POWER_BUTTON | SMC_EJECT_BUTTON);
    if(input & SMC_EJECT_BUTTON)
    {
        printf("Dumping SLC-RAW to FAT32...\n");
        res = _dump_slc_raw(NAND_BANK_SLC);
        if(res) {
            printf("Failed to dump SLC-RAW (%d)!\n", res);
            goto format_exit;
        }

        printf("Dumping SLCCMPT-RAW to FAT32...\n");
        res = _dump_slc_raw(NAND_BANK_SLCCMPT);
        if(res) {
            printf("Failed to dump SLCCMPT-RAW (%d)!\n", res);
            goto format_exit;
        }
    }

    u32 mlc_base = LD_DWORD(&part3[0x8]);
    u32 slc_base = LD_DWORD(&part4[0x8]);
    u32 slccmpt_base = slc_base + ((NAND_MAX_PAGE * PAGE_SIZE) / SDMMC_DEFAULT_BLOCKLEN);

    printf("Dumping redNAND...\n");
    res = _dump_copy_rednand(slc_base, slccmpt_base, mlc_base);
    if(res) {
        printf("Failed to dump redNAND (%d)!\n", res);
        goto format_exit;
    }

format_exit:
    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

#endif // MINUTE_BOOT1
