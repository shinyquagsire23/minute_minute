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
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "sdmmc.h"
#include "sdhc.h"
#include "sdcard.h"
#include "mlc.h"
#include "nand.h"
#include "isfs.h"
#include "main.h"
#include "prsh.h"
#include "latte.h"
#include "ancast.h"
#include "seeprom.h"
#include "crc32.h"
#include "mbr.h"
#include "rednand.h"
#include "ppc.h"

#include "ff.h"

#include "smc.h"
#include "crypto.h"

#ifndef MINUTE_BOOT1
#ifndef FASTBOOT

// TODO: how many sectors is 8gb MLC WFS?
#define TOTAL_SECTORS (0x3A20000)

extern seeprom_t seeprom;
extern otp_t otp;

extern void boot1_prshhax_payload(void);
extern void boot1_prshhax_payload_end(void);

void dump_set_sata_type(void);
void dump_set_sata_type_1(void);
void dump_set_sata_type_2(void);
void dump_set_sata_type_3(void);
void dump_set_sata_type_4(void);
void dump_set_sata_type_5(void);
void dump_set_sata_type_6(void);
void dump_set_sata_type_7(void);
void dump_set_sata_type_8(void);

static u8 nand_page_buf[PAGE_SIZE + PAGE_SPARE_SIZE] ALIGNED(NAND_DATA_ALIGN);
static u8 nand_ecc_buf[ECC_BUFFER_ALLOC] ALIGNED(NAND_DATA_ALIGN);

menu menu_dump = {
    "minute", // title
    {
            "Backup and Restore", // subtitles
    },
    1, // number of subtitles
    {
            {"Dump SEEPROM & OTP", &dump_seeprom_otp},
            {"Dump Espresso OTP & bootrom", &dump_espresso},
            {"Dump OTP via PRSHhax", &dump_otp_via_prshhax},
            {"Dump SLC.RAW", &dump_slc_raw},
            {"Dump SLCCMPT.RAW", &dump_slccmpt_raw},
            {"Dump BOOT1_SLC.RAW", &dump_boot1_raw},
            {"Dump BOOT1_SLCCMPT.RAW", &dump_boot1_vwii_raw},
            {"Dump factory log", &dump_factory_log},
            {"Dump sys crash logs", &dump_logs_slc},
            {"Dump sys crash logs from redslc", &dump_logs_redslc},
            {"Format redNAND", &dump_format_rednand},
            {"Restore SLC.RAW", &dump_restore_slc_raw},
            {"Restore SLCCMPT.RAW", &dump_restore_slccmpt_raw},
            {"Restore BOOT1_SLC.RAW", &dump_restore_boot1_raw},
            {"Restore BOOT1_SLCCMPT.RAW", &dump_restore_boot1_vwii_raw},
            {"Restore BOOT1_SLC.IMG", &dump_restore_boot1_img},
            {"Restore BOOT1_SLCCMPT.IMG", &dump_restore_boot1_vwii_img},
            {"Restore seeprom.bin", &dump_restore_seeprom},
            {"Erase MLC", &dump_erase_mlc},
            {"Delete scfm.img", &_dump_delete_scfm},
            {"Delete SLCCMPT scfm.img", &_dump_delete_scfm_slccmpt},
            {"Delete redNAND scfm.img", &_dump_delete_scfm_rednand},
            {"Restore redNAND MLC", &dump_restore_rednand},
            {"Sync SEEPROM boot1 versions with NAND", &dump_sync_seeprom_boot1_versions},
            {"Set SEEPROM SATA device type", &dump_set_sata_type},
            {"Test SLC and Restore SLC.RAW", &dump_restore_test_slc_raw},
            {"Print SLC superblocks", &dump_print_slc_superblocks},
            {"Return to Main Menu", &menu_close},
    },
    28, // number of options
    0,
    0
};

menu menu_sata = {
    "minute", // title
    {
            "Set SEEPROM SATA device type", // subtitles
    },
    1, // number of subtitles
    {
            {"Default", &dump_set_sata_type_1},
            {"No device", &dump_set_sata_type_2},
            {"ROM drive (Retail)", &dump_set_sata_type_3},
            {"R drive (Test/Kiosk CAT-I)", &dump_set_sata_type_4},
            {"MION (Debug)", &dump_set_sata_type_5},
            {"SES (Kiosk CAT-SES)", &dump_set_sata_type_6},
            {"GEN2-HDD (Kiosk CAT-I with HDD)", &dump_set_sata_type_7},
            {"GEN1-HDD (Kiosk CAT-I with HDD)", &dump_set_sata_type_8},
            {"Cancel and return", &menu_close},
    },
    9, // number of options
    0,
    0
};

const char* _dump_sata_type_str(int type)
{
    switch (type)
    {
        case 0:
            return "Unk0";
        case SATA_TYPE_DEFAULT:
            return "Default";
        case SATA_TYPE_NODRIVE:
            return "No device";
        case SATA_TYPE_ROMDRIVE:
            return "ROM drive (Retail)";
        case SATA_TYPE_RDRIVE:
            return "R drive (Test/Kiosk CAT-I)";
        case SATA_TYPE_MION:
            return "MION (Debug)";
        case SATA_TYPE_SES:
            return "SES (Kiosk CAT-SES)";
        case SATA_TYPE_GEN2HDD:
            return "GEN2-HDD (Kiosk CAT-I with HDD)";
        case SATA_TYPE_GEN1HDD:
            return "GEN1-HDD (Kiosk CAT-I with HDD)";
        default:
            return "Unk";
    }
}

void dump_menu_show()
{
    menu_init(&menu_dump);
}

void dump_factory_log()
{
    FILE* f_log = NULL;
    int ret = 0;

    if(mlc_init())
        goto close_ret;

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

    console_power_or_eject_to_return();
}

int mandatory_seeprom_otp_backups()
{
    char tmp[128];
    FILE* f_otp = NULL;
    FILE* f_eep = NULL;

    int ret = 1;

    printf("Making mandatory OTP/SEEPROM backups...\n");

    for (int try = 0; try < 128; try++)
    {
        snprintf(tmp, 127, "sdmc:/backup_otp_%u.bin", try);
        tmp[127] = 0;

        f_otp = fopen(tmp, "rb");
        if (f_otp) {
            fclose(f_otp);
            continue;
        }
        f_otp = fopen(tmp, "wb");
        if (f_otp) break;
    }

    printf("Dumping OTP to `%s`...\n", tmp);
    if(!f_otp)
    {
        printf("Failed to open `%s`.\n", tmp);
        ret = 0;
    }
    else {
        fwrite(&otp, 1, sizeof(otp_t), f_otp);
        fclose(f_otp);
    }

    for (int try = 0; try < 128; try++)
    {
        snprintf(tmp, 127, "sdmc:/backup_seeprom_%u.bin", try);
        tmp[127] = 0;

        f_eep = fopen(tmp, "rb");
        if (f_eep) {
            fclose(f_eep);
            continue;
        }
        f_eep = fopen(tmp, "wb");
        if (f_eep) break;
    }

    printf("Dumping SEEPROM to `%s`...\n", tmp);
    if(!f_eep)
    {
        printf("Failed to open `%s`.\n", tmp);
        ret = 0;
    }
    else {
        fwrite(&seeprom, 1, sizeof(seeprom_t), f_eep);
        fclose(f_eep);
    }

    return ret;
}

void dump_seeprom_otp(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char* otp_path = "sdmc:/otp.bin";
    if (crypto_otp_is_de_Fused) {
        otp_path = "sdmc:/de_Fuse_otp.bin";
    }

    printf("Dumping OTP to `%s`...\n", otp_path);
    FILE* f_otp = fopen(otp_path, "wb");
    if(!f_otp)
    {
        printf("Failed to open `%s`.\n", otp_path);
        goto ret;
    }
    fwrite(&otp, 1, sizeof(otp_t), f_otp);
    fclose(f_otp);

    printf("Dumping SEEPROM to `sdmc:/seeprom.bin`...\n");
    FILE* f_eep = fopen("sdmc:/seeprom.bin", "wb");
    if(!f_eep)
    {
        printf("Failed to open sdmc:/seeprom.bin.\n");
        goto ret;
    }
    fwrite(&seeprom, 1, sizeof(seeprom_t), f_eep);
    fclose(f_eep);

    if (memcmp(&seeprom, &seeprom_decrypted, sizeof(seeprom)))
    {
        printf("Dumping decrypted SEEPROM to `sdmc:/seeprom_decrypted.bin`...\n");
        FILE* f_eep = fopen("sdmc:/seeprom_decrypted.bin", "wb");
        if(!f_eep)
        {
            printf("Failed to open sdmc:/seeprom_decrypted.bin.\n");
            goto ret;
        }
        fwrite(&seeprom_decrypted, 1, sizeof(seeprom_t), f_eep);
        fclose(f_eep);
    }

    printf("\nDone!\n");
ret:
    console_power_or_eject_to_return();
}

void dump_espresso(void)
{
    gfx_clear(GFX_ALL, BLACK);

    ppc_dump_bootrom_otp();

ret:
    console_power_or_eject_to_return();
}

void _dump_sync_seeprom_boot1_versions(void)
{
    seeprom_t crypt_verify;
    seeprom_t readback_verify;

    crypto_read_seeprom();

    if (!crypto_decrypt_verify_seeprom_ptr(&seeprom_decrypted, &seeprom)) {
        printf("\nSEEPROM failed to verify!\n");
        printf("boot1 version cannot be written to SEEPROM without a valid otp.bin!\n");
        goto ret;
    }

    int needs_sync = 0;

    u16 original_version_1 = seeprom_decrypted.boot1_params.version;
    u16 original_version_2 = seeprom_decrypted.boot1_copy_params.version;

    {
        int bank = NAND_BANK_SLC;
        if (!(seeprom_decrypted.boot1_params.sector >> 12)) {
            bank = NAND_BANK_SLCCMPT;
        }
        int page = (seeprom_decrypted.boot1_params.sector & 0xFFF) * 0x40;
        nand_initialize(bank);
        nand_read_page(page, nand_page_buf, nand_ecc_buf);

        ancast_header* hdr = (ancast_header*)(nand_page_buf + 0x1A0);

        if (hdr->version == 0xFFFFFFFF || !hdr->version) {
            printf("Refusing to sync NAND boot1 version 0x%04x (erased NAND page?)\n");
        }
        else if (seeprom_decrypted.boot1_params.version != hdr->version) {
            printf("\nSEEPROM boot1 version v%u does not match NAND version v%u!\n", seeprom_decrypted.boot1_params.version, hdr->version);
            printf("Change SEEPROM boot1 version from v%u to v%u?\n", seeprom_decrypted.boot1_params.version, hdr->version);
            if(console_abort_confirmation_power_no_eject_yes()) return;

            needs_sync = 1;
            seeprom_decrypted.boot1_params.version = hdr->version;
        }
    }

    {
        int bank = NAND_BANK_SLC;
        if (!(seeprom_decrypted.boot1_copy_params.sector >> 12)) {
            bank = NAND_BANK_SLCCMPT;
        }
        int page = (seeprom_decrypted.boot1_copy_params.sector & 0xFFF) * 0x40;
        nand_initialize(bank);
        nand_read_page(page, nand_page_buf, nand_ecc_buf);


        ancast_header* hdr = (ancast_header*)(nand_page_buf + 0x1A0);

        if (hdr->version == 0xFFFFFFFF || !hdr->version) {
            printf("Refusing to sync NAND boot1 version 0x%04x (erased NAND page?)\n");
        }
        else if (seeprom_decrypted.boot1_copy_params.version != hdr->version) {
            printf("\nSEEPROM boot1 version v%u does not match NAND version v%u!\n", seeprom_decrypted.boot1_copy_params.version, hdr->version);
            printf("Change SEEPROM boot1 version from v%u to v%u?\n", seeprom_decrypted.boot1_copy_params.version, hdr->version);

            if(console_abort_confirmation_power_no_eject_yes()) return;

            needs_sync = 1;
            seeprom_decrypted.boot1_copy_params.version = hdr->version;
        }
    }

    if (!needs_sync) {
        printf("SEEPROM boot1 versions are already synced with NAND\n");
        return;
    }

    seeprom_decrypted.hw_params_crc32 = crc32(&seeprom_decrypted.hw_params, 0xC);
    seeprom_decrypted.boot1_params_crc32 = crc32(&seeprom_decrypted.boot1_params, 0xC);
    seeprom_decrypted.boot1_copy_params_crc32 = crc32(&seeprom_decrypted.boot1_copy_params, 0xC);

    if (!mandatory_seeprom_otp_backups()) {
        printf("The mandatory SEEPROM/OTP backups are mandatory.\n");
        goto ret;
    }
    printf("Done making mandatory backups.\n");

    if (!crypto_encrypt_verify_seeprom_ptr(&seeprom, &seeprom_decrypted)) {
        goto ret;
    }
    printf("Done verifying.\n");

    {
        printf("\n\nSEEPROM will be synced to the following versions:\n");
        printf("Primary:   v%u -> v%u\n", original_version_1, seeprom_decrypted.boot1_params.version);
        printf("Secondary: v%u -> v%u\n", original_version_2, seeprom_decrypted.boot1_copy_params.version);
        printf("Write these values to SEEPROM?\n");

        if(console_abort_confirmation_power_no_eject_yes()) return;
    }

    //seeprom_write(&seeprom.hw_params, (((u32)&seeprom.hw_params) - ((u32)&seeprom) / 2), 0x30/2);
    seeprom_write(&seeprom, 0, sizeof(seeprom)/2);
    udelay(10);
    seeprom_read(&readback_verify, 0, sizeof(readback_verify)/2);

    int has_issues = 0;
    if (memcmp(&seeprom, &readback_verify, sizeof(seeprom))) {
        printf("\nSEEPROM write failed!\n");
        printf("Readback did not match!\n");
        printf("Rename your mandatory backup to `seeprom.bin` and flash it ASAP.\n");

        printf("Read back:");
        u8* printout = (u8*)&readback_verify;
        for (int i = 0; i < sizeof(readback_verify); i++)
        {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%02x ", *printout++);
        }
        printf("\n");

        printf("Expected:");
        printout = (u8*)&seeprom;
        for (int i = 0; i < sizeof(seeprom); i++)
        {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%02x ", *printout++);
        }
        printf("\n");
        has_issues = 1;
    }

    if (!crypto_decrypt_verify_seeprom_ptr(&crypt_verify, &readback_verify)) {
        printf("\nSEEPROM CRC32s failed to verify!\n");
        printf("This unit might not boot up without de_Fuse now...\n");
        printf("Rename your mandatory backup to `seeprom.bin` and flash it ASAP.\n");
        has_issues = 1;
    }

    if (!has_issues) {
        printf("\nSuccess!\n");
    }
    else {
        printf("\nDone.\n");
    }
    return;

ret:
    return;
}

void dump_sync_seeprom_boot1_versions(void)
{
    gfx_clear(GFX_ALL, BLACK);

    _dump_sync_seeprom_boot1_versions();

    console_power_or_eject_to_return();
}

void _dump_set_sata_type(int new_sata)
{
    seeprom_t crypt_verify;
    seeprom_t readback_verify;

    gfx_clear(GFX_ALL, BLACK);

    crypto_read_seeprom();

    if (!crypto_decrypt_verify_seeprom_ptr(&seeprom_decrypted, &seeprom)) {
        printf("\nSEEPROM failed to verify!\n");
        printf("boot1 version cannot be written to SEEPROM without a valid otp.bin!\n");
        goto ret;
    }

    int needs_sync = 0;

    u16 original_sata = seeprom_decrypted.bc.sata_device;
    seeprom_decrypted.bc.sata_device = new_sata; // none TODO

    seeprom_decrypted.bc_crc32 = crc32(&seeprom_decrypted.bc_size, seeprom_decrypted.bc_size - sizeof(u32));

    if (!mandatory_seeprom_otp_backups()) {
        printf("The mandatory SEEPROM/OTP backups are mandatory.\n");
        goto ret;
    }
    printf("Done making mandatory backups.\n");

    if (!crypto_encrypt_verify_seeprom_ptr(&seeprom, &seeprom_decrypted)) {
        goto ret;
    }
    printf("Done verifying.\n");

    {
        printf("\n\nSEEPROM will be set to the following SATA devices:\n");
        printf("Primary:   %u (%s) -> %u (%s)\n", original_sata, _dump_sata_type_str(original_sata), seeprom.bc.sata_device, _dump_sata_type_str(seeprom.bc.sata_device));
        printf("Write these values to SEEPROM?\n");

        if(console_abort_confirmation_power_no_eject_yes()) return;
    }

    //seeprom_write(&seeprom.hw_params, (((u32)&seeprom.hw_params) - ((u32)&seeprom) / 2), 0x30/2);
    seeprom_write(&seeprom, 0, sizeof(seeprom)/2);
    udelay(10);
    seeprom_read(&readback_verify, 0, sizeof(readback_verify)/2);

    int has_issues = 0;
    if (memcmp(&seeprom, &readback_verify, sizeof(seeprom))) {
        printf("\nSEEPROM write failed!\n");
        printf("Readback did not match!\n");
        printf("Rename your mandatory backup to `seeprom.bin` and flash it ASAP.\n");

        printf("Read back:");
        u8* printout = (u8*)&readback_verify;
        for (int i = 0; i < sizeof(readback_verify); i++)
        {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%02x ", *printout++);
        }
        printf("\n");

        printf("Expected:");
        printout = (u8*)&seeprom;
        for (int i = 0; i < sizeof(seeprom); i++)
        {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%02x ", *printout++);
        }
        printf("\n");
        has_issues = 1;
    }

    if (!crypto_decrypt_verify_seeprom_ptr(&crypt_verify, &readback_verify)) {
        printf("\nSEEPROM CRC32s failed to verify!\n");
        printf("This unit might not boot up without de_Fuse now...\n");
        printf("Rename your mandatory backup to `seeprom.bin` and flash it ASAP.\n");
        has_issues = 1;
    }

    if (!has_issues) {
        printf("\nSuccess!\n");
    }
    else {
        printf("\nDone.\n");
    }

    menu_close();
    console_power_or_eject_to_return();
    return;

ret:
    menu_close();
    console_power_or_eject_to_return();
    return;
}

void dump_set_sata_type(void)
{
    menu_init(&menu_sata);
}

void dump_set_sata_type_1(void)
{
    _dump_set_sata_type(1);
}

void dump_set_sata_type_2(void)
{
    _dump_set_sata_type(2);
}

void dump_set_sata_type_3(void)
{
    _dump_set_sata_type(3);
}

void dump_set_sata_type_4(void)
{
    _dump_set_sata_type(4);
}

void dump_set_sata_type_5(void)
{
    _dump_set_sata_type(5);
}

void dump_set_sata_type_6(void)
{
    _dump_set_sata_type(6);
}

void dump_set_sata_type_7(void)
{
    _dump_set_sata_type(7);
}

void dump_set_sata_type_8(void)
{
    _dump_set_sata_type(8);
}

void dump_restore_seeprom(void)
{
    gfx_clear(GFX_ALL, BLACK);

    seeprom_t to_write;
    seeprom_t crypt_verify;
    seeprom_t readback_verify;

    if (!mandatory_seeprom_otp_backups()) {
        printf("The mandatory SEEPROM/OTP backups are mandatory.\n");
        goto ret;
    }

    {
        printf("Write sdmc:/seeprom.bin to SEEPROM?\n");
        if(console_abort_confirmation_power_no_eject_yes()) return;
    }

    printf("Restoring SEEPROM from `sdmc:/seeprom.bin`...\n");
    FILE* f_eep = fopen("sdmc:/seeprom.bin", "rb");
    if(!f_eep)
    {
        printf("Failed to open sdmc:/seeprom.bin.\n");
        goto ret;
    }
    if (fread(&to_write, 1, sizeof(seeprom_t), f_eep) != sizeof(seeprom_t)) {
        fclose(f_eep);

        printf("sdmc:/seeprom.bin is the wrong size!! Aborting.\n");
        goto ret;
    }
    fclose(f_eep);

    printf("Verifying seeprom.bin...\n");
    if (!crypto_decrypt_verify_seeprom_ptr(&crypt_verify, &to_write)) {
        printf("\nSEEPROM failed to verify!\n");
        printf("(A valid otp.bin is required)\n");
        goto ret;
    }
    else {
        printf("Everything verified!\n\n");

        printf("Last chance: Are you sure you want to write SEEPROM?\n");
        printf("If you write an invalid SEEPROM and then fail to backup\n");
        printf("otp.bin, you will NOT be able to recover your Wii U!\n\n");

        printf("A missing otp.bin can ONLY be recovered with a valid\n");
        printf("seeprom.bin from the SAME Wii U, and a missing seeprom.bin\n");
        printf("can ONLY be partially recovered (enough to boot) with a\n");
        printf(" valid otp.bin from the SAME Wii U!\n\n");

        printf("If you lose BOTH otp.bin and seeprom.bin, you will be FORCED to\n");
        printf("use a donor copy from another Wii U.\n");
        printf("This *may* mean forfeiting the ability to play online!\n");
        printf("This WILL mean saves stored on NAND or USBs will be unrecoverable!\n");
        printf("This WILL mean your disk drive will no longer be usable!\n\n");

        printf("This is like, the one limitation of de_Fuse lol.\n");
        printf("You probably don't want to be here unless you're a developer\n");
        printf("and know what you're doing.\n\n");

        smc_get_events(); // Eat all existing events
        udelay(3000*1000);
        smc_get_events(); // Eat all existing events

        printf("Write sdmc:/seeprom.bin to SEEPROM?\n");
        if(console_konami_code()) return;
    }

    {
        int bank = NAND_BANK_SLC;
        if (!(crypt_verify.boot1_params.sector >> 12)) {
            bank = NAND_BANK_SLCCMPT;
        }
        int page = (crypt_verify.boot1_params.sector & 0xFFF) * 0x40;
        nand_initialize(bank);
        nand_read_page(page, nand_page_buf, nand_ecc_buf);

        ancast_header* hdr = (ancast_header*)(nand_page_buf + 0x1A0);

        if (crypt_verify.boot1_params.version != hdr->version) {
            printf("WARNING: SEEPROM boot1 version v%u does not match NAND version v%u!\n", crypt_verify.boot1_params.version, hdr->version);
            printf("Continue writing sdmc:/seeprom.bin to SEEPROM?\n");
            if(console_abort_confirmation_power_no_eject_yes()) return;
        }
    }

    {
        int bank = NAND_BANK_SLC;
        if (!(crypt_verify.boot1_copy_params.sector >> 12)) {
            bank = NAND_BANK_SLCCMPT;
        }
        int page = (crypt_verify.boot1_copy_params.sector & 0xFFF) * 0x40;
        nand_initialize(bank);
        nand_read_page(page, nand_page_buf, nand_ecc_buf);

        ancast_header* hdr = (ancast_header*)(nand_page_buf + 0x1A0);

        if (crypt_verify.boot1_copy_params.version != hdr->version) {
            printf("WARNING: SEEPROM boot1 version v%u does not match NAND version v%u!\n", crypt_verify.boot1_copy_params.version, hdr->version);
            printf("Continue writing sdmc:/seeprom.bin to SEEPROM?\n");
            if(console_abort_confirmation_power_no_eject_yes()) return;
        }
    }
    
    seeprom_write(&to_write, 0, sizeof(to_write)/2);
    seeprom_read(&readback_verify, 0, sizeof(readback_verify)/2);

    int has_issues = 0;
    if (memcmp(&to_write, &readback_verify, sizeof(to_write))) {
        printf("SEEPROM write failed!\n");
        printf("Readback did not match!\n");
        has_issues = 1;
    }
    else {
        memcpy(&seeprom, &to_write, sizeof(to_write)); // Update RAM copy
    }

    if (!crypto_decrypt_verify_seeprom_ptr(&crypt_verify, &readback_verify)) {
        printf("SEEPROM CRC32s failed to verify!\n");
        printf("This unit might not boot up without de_Fuse now...\n");
        has_issues = 1;
    }

    if (!has_issues) {
        printf("\nSuccess!\n");
    }
    else {
        printf("\nDone.\n");
    }

ret:
    console_power_or_eject_to_return();
}

int _dump_mlc(u32 base)
{
    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    if(mlc_init())
        return -1;

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
    for(u32 sector = SDHC_BLOCK_COUNT_MAX; sector < TOTAL_SECTORS; sector += SDHC_BLOCK_COUNT_MAX)
    {
        int complete = 0;
        // Make sure to retry until the command succeeded, probably superfluous but harmless...
        while(complete != 0b11) {
            // Issue commands if we didn't already complete them.
            if(!(complete & 0b01))
                mres = mlc_start_read(sector, SDHC_BLOCK_COUNT_MAX, mlc_buf, &mlc_cmd);
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

int _dump_restore_mlc(u32 base)
{
    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    if(mlc_init())
        return -2;

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
    do res = sdcard_read(base, SDHC_BLOCK_COUNT_MAX, mlc_buf);
    while(res);

    // Read first block from MLC to compare against for safety checks.
    do res = mlc_read(0, SDHC_BLOCK_COUNT_MAX, sdcard_buf);
    while(res);

    bool allzero = true;
    for(size_t i = 0; i < SDMMC_DEFAULT_BLOCKLEN * SDHC_BLOCK_COUNT_MAX; i++){
        if(mlc_buf[i]){
            allzero = false;
            break;
        }
    }
    // Check to see if the first block matches, if so, ask the user if they want to continue.
    if(allzero){
        printf("MLC: First block is empty, continue restoring?\n");
    } else if(memcmp(sdcard_buf, mlc_buf, SDMMC_DEFAULT_BLOCKLEN * SDHC_BLOCK_COUNT_MAX) == 0) {
        printf("MLC: First blocks match, continue restoring?\n");
    } else {
        printf("MLC: First blocks do not match!\n");
        printf("MLC: Aborting restore.\n");
        return -3;
    }
    if(console_abort_confirmation_power_no_eject_yes()) 
        return -4;
    printf("MLC: Continuing restore...\n");

    // Do one less iteration than we need, due to having to special case the start and end.
    u32 sdcard_sector = base + SDHC_BLOCK_COUNT_MAX;
    u32 mlc_sector = 0;

    while(mlc_sector < (TOTAL_SECTORS - SDHC_BLOCK_COUNT_MAX))
    {
        int complete = 0;
        int retries = 0;
        // Make sure to retry until the command succeeded, probably superfluous but harmless...
        while(complete != 0b11) {
            // Issue commands if we didn't already complete them.
            if(!(complete & 0b01))
                sres = sdcard_start_read(sdcard_sector, SDHC_BLOCK_COUNT_MAX, sdcard_buf, &sdcard_cmd);
            if(!(complete & 0b10))
                mres = mlc_start_write(mlc_sector, SDHC_BLOCK_COUNT_MAX, mlc_buf, &mlc_cmd);

            // Only end the command if starting it succeeded.
            // If starting and ending the command succeeds, mark it as complete.
            if(!(complete & 0b01) && sres == 0) {
                sres = sdcard_end_read(&sdcard_cmd);
                if(sres == 0) complete |= 0b01;
            }
            if(!(complete & 0b10) && mres == 0) {
                mres = mlc_end_write(&mlc_cmd);
                if(mres == 0) complete |= 0b10;
            }

            if (retries > 9999999) {
                printf("MLC: Still working on sector 0x%08lX\n", mlc_sector);
                retries = 0;
            }

            retries++;
        }

        // Swap buffers.
        if(mlc_buf == sector_buf1) {
            mlc_buf = sector_buf2;
            sdcard_buf = sector_buf1;
        } else {
            mlc_buf = sector_buf1;
            sdcard_buf = sector_buf2;
        }

        if((mlc_sector % 0x10000) == 0) {
            printf("MLC: Sector 0x%08lX written\n", mlc_sector);
        }

        sdcard_sector += SDHC_BLOCK_COUNT_MAX;
        mlc_sector += SDHC_BLOCK_COUNT_MAX;
    }

    // Finish up the last iteration.
    do res = mlc_write(mlc_sector, SDHC_BLOCK_COUNT_MAX, mlc_buf);
    while(res);

    free(sector_buf1);
    free(sector_buf2);

    return 0;
}

int _dump_slc_raw(u32 bank, int boot1_only)
{
    #define PAGES_PER_ITERATION (0x10)
    #define TOTAL_ITERATIONS ((boot1_only ? BOOT1_MAX_PAGE : NAND_MAX_PAGE) / PAGES_PER_ITERATION)

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
    if (!boot1_only) {
        sprintf(path, "%s.RAW", name);
    }
    else {
        sprintf(path, "BOOT1_%s.RAW", name);
    }

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
            nand_read_page(page_base + page, nand_page_buf, nand_ecc_buf);
            nand_correct(page_base + page, nand_page_buf, nand_ecc_buf);

            memcpy(file_buf[page], nand_page_buf, PAGE_SIZE);
            memcpy(file_buf[page] + PAGE_SIZE, nand_ecc_buf, PAGE_SPARE_SIZE);
        }

        fres = f_write(&file, file_buf, sizeof(file_buf), &btx);
        if(fres != FR_OK || btx != sizeof(file_buf)) {
            f_close(&file);
            printf("Failed to write %s (%d).\n", path, fres);
            return -4;
        }

        if((i % 0x100) == 0) {
            printf("%s-RAW: Page 0x%05lX / 0x%05lX completed\n", name, page_base, PAGES_PER_ITERATION * TOTAL_ITERATIONS);
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

void _dump_print_superblocks(int volume){
    printf("Initializing...\n");
    isfs_init(volume);
    gfx_clear(GFX_ALL, BLACK);
    isfs_ctx *slc = isfs_get_volume(volume);
    isfshax_super *superblock = memalign(NAND_DATA_ALIGN, ISFSSUPER_SIZE);
    for(int slot=0; slot<slc->super_count; slot++){
        if(slot == 32){
            console_power_to_continue();
            gfx_clear(GFX_ALL, BLACK);
        }
        int res = isfs_read_super(slc, superblock, slot);
        if(res < 0){
            printf("Slot %d: READ ERROR %d\n", slot, res);
            continue;
        }
        int magic[2] = { *(int*)superblock->magic };
        printf("Slot %d: generation: 0x%08X, magic: 0x%08X (%4s)\n", slot, 
                superblock->generation, magic[0], magic);
        if(superblock->generation>=ISFSHAX_GENERATION_FIRST){
            printf(" isfshax: gen: 0x%08X, genbase: 0x%08X, index: 0x%08X, magic: 0x%08X, slots: [0x%x, 0x%x, 0x%x, 0x%x]\n", 
            superblock->isfshax.generation, superblock->isfshax.generationbase,
            superblock->isfshax.index, superblock->isfshax.magic, *(u8*)(superblock->isfshax.slots),
            *(u8*)(superblock->isfshax.slots+1),*(u8*)(superblock->isfshax.slots+2),*(u8*)(superblock->isfshax.slots+3));
        }
    }
    console_power_to_continue();
}

void dump_print_slc_superblocks(void){
    _dump_print_superblocks(ISFSVOL_SLC);
}

static bool check_all32(u8* arr, u32 length, u8 value){
    for(u32 i=0; i<length; i++){
        if(arr[i] != value)
            return true;
    }
    return false;
}

static u8* dump_get_new_super(FIL *f, isfs_ctx *ctx, bool *same_slots){
    isfs_ctx file_ctx = *ctx;
    file_ctx.file = f;
    file_ctx.super = memalign(NAND_DATA_ALIGN, ISFSSUPER_SIZE);
    int res = isfs_load_super(&file_ctx);
    if(res){
        free(file_ctx.super);
        return NULL;
    }
    *same_slots = false;
    if(file_ctx.isfshax){
        *same_slots = !memcmp(file_ctx.isfshax_slots, ctx->isfshax_slots, ISFSHAX_REDUNDANCY);
    }
    for(int i=0; i<ISFSHAX_REDUNDANCY; i++){
        isfs_super_mark_slot(&file_ctx, file_ctx.isfshax_slots[i], FAT_CLUSTER_RESERVED);
    }
    for(int i=0; i<ISFSHAX_REDUNDANCY; i++){
        isfs_super_mark_slot(&file_ctx, file_ctx.isfshax_slots[i], FAT_CLUSTER_BAD);
    }

    return file_ctx.super;
}

int _dump_restore_slc_raw(u32 bank, int boot1_only, bool nand_test)
{
    int ret = 0;
    int boot1_is_half = 0;

    #define PAGE_STRIDE (PAGE_SIZE + PAGE_SPARE_SIZE)
    #define FILE_BUF_SIZE (BLOCK_PAGES * PAGE_STRIDE)


    static u8 page_buf[PAGE_STRIDE] ALIGNED(64);
    static u8 file_buf[FILE_BUF_SIZE];

    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    isfs_ctx *ctx = NULL;
    bool protect_isfshax = false;
    const char* name = NULL;
    u32 boot1_page, boot1_copy_page;
    switch(bank) {
        case NAND_BANK_SLC: name = "SLC";
        isfs_init(ISFSVOL_SLC);
        if(!isfs_slc_has_isfshax_installed() && !crypto_otp_is_de_Fused){
            printf("SLC Restore not allowed!\nNeither ISFShax nor defuse is detected\nSLC restore could brick the consolse.\n");
            return -5;
        } 
        if(!crypto_otp_is_de_Fused){
            printf("Defuse not detected. boot1 and ISFShax superblocks will be protected during restore\nISFShax Superblocks: ");
            ctx = isfs_get_volume(ISFSVOL_SLC);
            for(int i = 0; i<ISFSHAX_REDUNDANCY; i++)
                printf("%u ", ctx->isfshax_slots[i]);
            printf("\n");
            protect_isfshax = true;
            boot1_page = (seeprom_decrypted.boot1_params.sector & 0xFFF) * 0x40;
            boot1_copy_page = (seeprom_decrypted.boot1_copy_params.sector & 0xFFF) * 0x40;
            printf("boot1 pages: 0x%lX and 0x%lX\n", boot1_page, boot1_copy_page);
        }
        
        break;
        case NAND_BANK_SLCCMPT: name = "SLCCMPT"; break;
        default: return -2;
    }

    char path[64] = {0};

    if (!boot1_only) {
        sprintf(path, "%s.RAW", name);
    }
    else {
        sprintf(path, "BOOT1_%s.RAW", name);
    }
 
    // Make sure the user is dedicated (and require a difficult button press)
    smc_get_events(); // Eat all existing events
    printf("Write sdmc:/%s to %s?\n", path, name);
    if (console_abort_confirmation_power_no_eject_yes()) return 0;

    FIL file = {0}; FRESULT fres = 0; UINT btx = 0;
    fres = f_open(&file, path, FA_READ);
    if(fres != FR_OK) {
        printf("Failed to open %s (%d).\n", path, fres);
        return -3;
    }

    fres = f_read(&file, file_buf, PAGE_STRIDE, &btx);
    if(fres != FR_OK || btx != PAGE_STRIDE) {
        f_close(&file);
        printf("Failed to read %s (%d).\n", path, fres);
        return -4;
    }

    u64 nand_file_size_expected = (boot1_only ? BOOT1_MAX_PAGE : NAND_MAX_PAGE) * PAGE_STRIDE;
    u64 nand_file_size = f_size(&file);
    if (nand_file_size != nand_file_size_expected && boot1_only && nand_file_size * 2 == nand_file_size_expected) {
        nand_file_size_expected /= 2;
        boot1_is_half = 1;
    }
    if (nand_file_size != nand_file_size_expected) {
        printf("Invalid file size! Expected 0x%llx, got 0x%llx.\n", nand_file_size_expected, nand_file_size);
        return -3;
    }

    u32 total_pages = boot1_only ?(boot1_is_half ? BOOT1_MAX_PAGE/2 : BOOT1_MAX_PAGE) : NAND_MAX_PAGE;

    if(!protect_isfshax){
        printf("Unmounting ISFSs\n");
        isfs_fini();
    } else {
        bool same_slots;
        u8 *new_super = dump_get_new_super(&file, ctx, &same_slots);
        if(!new_super){
            printf("Error finding supberblock in %s\n", name);
            return -6;
        }
        if(same_slots){
            printf("isfshax superblocks line up\n");
        } else {
            printf("isfshax superblocks don't line up\n");
            total_pages -= ctx->super_count * ISFSSUPER_CLUSTERS * CLUSTER_PAGES;
            printf("Clean out superblocks\n");
            for(u32 slot=0; slot<ctx->super_count; slot++){
                if(isfs_is_isfshax_super(ctx, slot))
                    continue;
                u32 page = NAND_MAX_PAGE - (ctx->super_count - slot) * CLUSTER_PAGES * ISFSSUPER_CLUSTERS;
                nand_erase_block(page);
            }
            printf("Commit latest superblock\n");
            free(ctx->super);
            ctx->super = new_super;
            isfs_commit_super(ctx);
        }
    }

    fres = f_rewind(&file);
    if(fres != FR_OK) {
        f_close(&file);
        printf("Failed to rewind %s (%d).\n", path, fres);
        return -3;
    }

    printf("Initializing %s...\n", name);
    nand_initialize(bank);

    u32 program_test_failed = 0;
    u32 program_test_failed_blocks = 0;
    u32 erase_test_failed = 0;
    u32 erase_test_failed_blocks = 0;
    u32 program_failed = 0;


    for(u32 page_base=0; page_base < total_pages; page_base += BLOCK_PAGES){
        fres = f_read(&file, file_buf, FILE_BUF_SIZE, &btx);
        if(fres != FR_OK || btx != min(FILE_BUF_SIZE, (total_pages-page_base) * PAGE_STRIDE)) {
            f_close(&file);
            printf("Failed to read %s (%d).\n", path, fres);
            return -4;
        }

        if(protect_isfshax){
            if(page_base == boot1_page || page_base == boot1_copy_page)
                continue; // leave boot1 alone
            int super_start_page = NAND_MAX_PAGE - ctx->super_count * ISFSSUPER_CLUSTERS * CLUSTER_PAGES;
            if(page_base >= super_start_page){
                u32 super_slot = page_base / (CLUSTER_PAGES * ISFSSUPER_CLUSTERS);
                if(isfs_is_isfshax_super(ctx, super_slot))
                    continue;
            }
        }

        if(nand_test){
            bool is_badblock = false;
            //Test if page can be fully programmed to 0
            for(u32 page=0; page < BLOCK_PAGES; page++){
                memset32(nand_page_buf, 0, PAGE_SIZE);
                memset32(nand_ecc_buf, 0, PAGE_SPARE_SIZE);
                nand_write_page(page_base + page, nand_page_buf, nand_ecc_buf);
                memset32(nand_page_buf, 0xffffffff, PAGE_SIZE);
                memset32(nand_ecc_buf, 0xffffffff, PAGE_SPARE_SIZE);
                // This might not be optional? Bug?
                nand_read_page(page_base + page, nand_page_buf, nand_ecc_buf);
                if(check_all32(nand_page_buf, PAGE_SIZE, 0)){
                    printf("Page 0x%05lX failed program test\n", page_base + page);
                    program_test_failed++;
                    if(!is_badblock){
                        is_badblock = true;
                        program_test_failed_blocks++;
                    }
                }
                //nand_correct(page_base + page, nand_page_buf, nand_
            }
        }

        nand_erase_block(page_base);

        if(nand_test){
            bool is_badblock = false;
            // Test if page can be fully erased to ff
            for(u32 page=0; page < BLOCK_PAGES; page++){
                memset32(nand_page_buf, 0, PAGE_SIZE);
                memset32(nand_ecc_buf, 0, PAGE_SPARE_SIZE);
                // This might not be optional? Bug?
                nand_read_page(page_base + page, nand_page_buf, nand_ecc_buf);
                if(check_all32(nand_page_buf, PAGE_SIZE, 0xff)){
                    printf("Page 0x%05lX failed erase test\n", page_base + page);
                    erase_test_failed++;
                    if(!is_badblock){
                        is_badblock = true;
                        erase_test_failed_blocks++;
                    }
                }
                //nand_correct(page_base + page, nand_page_buf, nand_
            }
        }

        for(u32 page=0; page < BLOCK_PAGES; page++){
            memcpy(nand_page_buf, &file_buf[page*PAGE_STRIDE], PAGE_STRIDE);
            memcpy(nand_ecc_buf, &file_buf[(page*PAGE_STRIDE) + PAGE_SIZE], PAGE_SPARE_SIZE);
            memcpy(nand_ecc_buf+PAGE_SPARE_SIZE, nand_ecc_buf+PAGE_SPARE_SIZE-0x10, 0x10);

            int is_cleared = 1;
            for (int j = 0; j < PAGE_STRIDE; j++)
            {
                if (file_buf[(page*PAGE_STRIDE)+j] != 0xFF) {
                    is_cleared = 0;
                    break;
                }
            }

            // Don't need to program unprogrammed pages
            if (!is_cleared) {
                
                //nand_correct(page_base + page, nand_page_buf, nand_ecc_buf);
                nand_write_page_raw(page_base + page, nand_page_buf, nand_ecc_buf);
            }

            // This might not be optional? Bug?
            nand_read_page(page_base + page, nand_page_buf, nand_ecc_buf);
            //nand_correct(page_base + page, nand_page_buf, nand_ecc_buf);

            if (memcmp(nand_page_buf, &file_buf[page*PAGE_STRIDE], PAGE_STRIDE)) {
                printf("Failed to program page: 0x%05lX\n", page_base + page);
            }
        }

        if((page_base % (BLOCK_PAGES * 0x10)) == 0) 
        {
            printf("%s-RAW: Page 0x%05lX / 0x%05lX completed\n", name, page_base, total_pages);
        }

    }

    fres = f_close(&file);
    if(fres != FR_OK) {
        printf("Failed to close %s (%d).\n", path, fres);
        ret = -5;
    }

    if(nand_test){
        printf("%u pages in %u blocks failed program test\n", 
                    program_test_failed, program_test_failed_blocks);
        printf("%u pages in %u blocks failed erase test\n", 
                    erase_test_failed, erase_test_failed_blocks);
    }
    printf("%u pages failed to program\n", program_failed);

    _dump_sync_seeprom_boot1_versions();

    return ret;

    #undef FILE_BUF_SIZE
    #undef PAGE_STRIDE
}
// does not work for whole SLC, as the HMACs would be wrong
int _dump_restore_slc_img(u32 bank, int boot1_only)
{
    int ret = 0;
    int boot1_is_half = 0;

    #define FILE_BUF_SIZE (BLOCK_PAGES * PAGE_SIZE)

    static u8 file_buf[FILE_BUF_SIZE];

    sdcard_ack_card();
    if(sdcard_check_card() != SDMMC_INSERTED) {
        printf("SD card is not initialized.\n");
        return -1;
    }

    const char* name = NULL;
    isfs_ctx *ctx;
    switch(bank) {
        case NAND_BANK_SLC: 
            name = "SLC";
            ctx = isfs_get_volume(ISFSVOL_SLC); 
            break;
        case NAND_BANK_SLCCMPT: 
            name = "SLCCMPT";
            ctx = isfs_get_volume(ISFSVOL_SLCCMPT);
            break;
        default: return -2;
    }

    char path[64] = {0};
    if (!boot1_only) {
        sprintf(path, "%s.IMG", name);
    }
    else {
        sprintf(path, "BOOT1_%s.IMG", name);
    }

    // Make sure the user is dedicated (and require a difficult button press)
    smc_get_events(); // Eat all existing events
    printf("Write sdmc:/%s to %s?\n", path, name);
    if (console_abort_confirmation_power_no_eject_yes()) return 0;

    FIL file = {0}; FRESULT fres = 0; UINT btx = 0;
    fres = f_open(&file, path, FA_READ);
    if(fres != FR_OK) {
        printf("Failed to open %s (%d).\n", path, fres);
        return -3;
    }

    fres = f_read(&file, file_buf, PAGE_SIZE, &btx);
    if(fres != FR_OK || btx != PAGE_SIZE) {
        f_close(&file);
        printf("Failed to read %s (%d).\n", path, fres);
        return -4;
    }
    fres = f_rewind(&file);
    if(fres != FR_OK) {
        f_close(&file);
        printf("Failed to rewind %s (%d).\n", path, fres);
        return -3;
    }

    u64 nand_file_size_expected = (boot1_only ? BOOT1_MAX_PAGE : NAND_MAX_PAGE) * PAGE_SIZE;
    u64 nand_file_size = f_size(&file);
    if (nand_file_size != nand_file_size_expected && boot1_only && nand_file_size * 2 == nand_file_size_expected) {
        nand_file_size_expected /= 2;
        boot1_is_half = 1;
    }
    if (nand_file_size != nand_file_size_expected) {
        printf("Invalid file size! Expected 0x%llx, got 0x%llx.\n", nand_file_size_expected, nand_file_size);
        return -3;
    }

    printf("Initializing %s...\n", name);

    u32 program_failed = 0;

    const u32 total_pages = boot1_only ?(boot1_is_half ? BOOT1_MAX_PAGE/2 : BOOT1_MAX_PAGE) : NAND_MAX_PAGE;
    for(u32 cluster=0; cluster < total_pages / CLUSTER_PAGES; cluster += BLOCK_CLUSTERS){
        fres = f_read(&file, file_buf, FILE_BUF_SIZE, &btx);
        if(fres != FR_OK || btx != min(FILE_BUF_SIZE, (total_pages-cluster * CLUSTER_PAGES) * PAGE_SIZE)) {
            f_close(&file);
            printf("Failed to read %s (%d).\n", path, fres);
            return -4;
        }

        isfs_hmac_meta seed = { .cluster = cluster };
        int res = isfs_write_volume(ctx, cluster, BLOCK_CLUSTERS, ISFSVOL_FLAG_HMAC | ISFSVOL_FLAG_READBACK, &seed, file_buf);
        if(res){
            printf("Failed to program block: 0x%05lX\n", cluster / BLOCK_CLUSTERS);
            program_failed++;
        }
        if((cluster % (BLOCK_CLUSTERS * 0x10)) == 0) 
        {
            printf("%s: Page 0x%05lX / 0x%05lX completed\n", name, cluster * CLUSTER_PAGES, total_pages);
        }

    }

    fres = f_close(&file);
    if(fres != FR_OK) {
        printf("Failed to close %s (%d).\n", path, fres);
        ret = -5;
    }

    printf("%u pages failed to program\n", program_failed);

    _dump_sync_seeprom_boot1_versions();

    return ret;

    #undef FILE_BUF_SIZE
}

int _dump_slc_to_sdcard_sectors(u32 base, u32 bank)
{
    // how many sectors needed for a page (4)
    #define SECTORS_PER_PAGE (PAGE_SIZE / SDMMC_DEFAULT_BLOCKLEN)
    // the SD host controller can only transfer 512 sectors at a time (512*512 bytes)
    #define SECTORS_PER_ITERATION (SDHC_BLOCK_COUNT_MAX)
    // given the above constraint, the max number of SLC pages we can transfer to SD at a time (128)
    #define PAGES_PER_ITERATION (SECTORS_PER_ITERATION / SECTORS_PER_PAGE)
    // the number of SD transfer iterations required to complete the SLC dump (0x800)
    #define TOTAL_ITERATIONS (NAND_MAX_PAGE / PAGES_PER_ITERATION)

    static u8 page_buf[PAGES_PER_ITERATION][PAGE_SIZE] ALIGNED(NAND_DATA_ALIGN);

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
            nand_read_page(page_base + page, page_buf[page], nand_ecc_buf);
            nand_correct(page_base + page, page_buf[page], nand_ecc_buf);
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
        _dump_slc_to_sdcard_sectors(slc_base, NAND_BANK_SLC);
    }

    // Dump SLCCMPT.
    if(slccmpt_base != 0) {
        _dump_slc_to_sdcard_sectors(slccmpt_base, NAND_BANK_SLCCMPT);
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

    mbr_sector mbr ALIGNED(32) = {0};

    res = sdcard_read(0, 1, &mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        return -1;
    }

    u32 fat_base = LD_DWORD(mbr.partition[0].lba_start);
    u32 fat_sectors = LD_DWORD(mbr.partition[0].lba_length);
    u32 fat_end = fat_base + fat_sectors;

    const u32 slc_sectors = (NAND_MAX_PAGE * PAGE_SIZE) / SDMMC_DEFAULT_BLOCKLEN;
    const u32 mlc_sectors =  TOTAL_SECTORS; // TODO: 8GB model.
    const u32 data_sectors = 0x100000 / SDMMC_DEFAULT_BLOCKLEN;

    u32 end = (u32)sdcard_get_sectors() & 0xFFFF0000;
    u32 slccmpt_base = end - slc_sectors;
    u32 slc_base = slccmpt_base - slc_sectors;
    u32 mlc_base = slc_base - mlc_sectors;

    bool keep_fat = fat_end <= mlc_base;

    if(keep_fat){
        printf("Keeping first partition\n");
        printf("ALL DATA ON THE OTHER PARTITIONS WILL BE OVERWRITTEN!\n");
    } else {
        fat_sectors = mlc_base - 1 - data_sectors;
        fat_base = mlc_base - fat_sectors;
        printf("Repartition SD card?\n");
        printf("ALL DATA ON THE SD CARD WILL BE OVERWRITTEN!\n");
    }
    printf("THIS CANNOT BE UNDONE!\n");
    smc_get_events(); // Eat all existing events
    if(console_abort_confirmation_power_no_eject_yes()) return -1;

    printf("Partitioning SD card...\n");


    printf("Partition layout on SD with 0x%08lX (0x%08lX) sectors:\n", (u32)sdcard_get_sectors(), end);

    printf("FAT32:   0x%08lX->0x%08lX\n", fat_base, fat_base + fat_sectors);
    printf("MLC:     0x%08lX->0x%08lX\n", mlc_base, mlc_base + mlc_sectors);
    printf("SLC:     0x%08lX->0x%08lX\n", slc_base, slc_base + slc_sectors);
    printf("SLCCMPT: 0x%08lX->0x%08lX\n", slccmpt_base, slccmpt_base + slc_sectors);

    if(console_abort_confirmation_power_exit_eject_continue()) return 1;

    if(!keep_fat){
        printf("Formatting to FAT32...\n");
        fres = f_mkfs("sdmc:", 0, 0, fat_base, fat_base + fat_sectors);
        if(fres != FR_OK) {
            printf("Failed to format card (%d)!\n", fres);
            return -2;
        }
        res = sdcard_read(0, 1, &mbr);
        if(res) {
            printf("Failed to read MBR (%d)!\n", res);
            return -3;
        }
        mbr.partition[0].type = 0x0C; //FAT32
    }

    printf("Updating MBR...\n");

    memset(&mbr.partition[1], 0x00, sizeof(mbr.partition[1]));
    mbr.partition[1].type = MBR_PARTITION_TYPE_MLC;
    ST_DWORD(mbr.partition[1].lba_start, mlc_base);
    ST_DWORD(mbr.partition[1].lba_length,  mlc_sectors);

    memset(&mbr.partition[2], 0x00, sizeof(mbr.partition[2]));
    mbr.partition[2].type = MBR_PARTITION_TYPE_SLC;
    ST_DWORD(mbr.partition[2].lba_start, slc_base);
    ST_DWORD(mbr.partition[2].lba_length, slc_sectors);

    memset(&mbr.partition[3], 0x00, sizeof(mbr.partition[3]));
    mbr.partition[3].type = MBR_PARTITION_TYPE_SLCCMPT;
    ST_DWORD(mbr.partition[3].lba_start, slccmpt_base);
    ST_DWORD(mbr.partition[3].lba_length, slc_sectors);

    res = sdcard_write(0, 1, &mbr);
    if(res) {
        printf("Failed to write MBR (%d)!\n", res);
        return -4;
    }

    // Mandatory backup
    mandatory_seeprom_otp_backups();

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

    res = _dump_slc_to_sdcard_sectors(slc_base, NAND_BANK_SLC);
    if(res) {
        printf("Failed to dump SLC (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_slc_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Dumping SLC.RAW...\n");

    res = _dump_slc_raw(NAND_BANK_SLC, 0);
    if(res) {
        printf("Failed to dump SLC.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_slccmpt_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Dumping SLCCMPT.RAW...\n");

    res = _dump_slc_raw(NAND_BANK_SLCCMPT, 0);
    if(res) {
        printf("Failed to dump SLCCMPT.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_boot1_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Dumping BOOT1_SLC.RAW...\n");

    res = _dump_slc_raw(NAND_BANK_SLC, 1);
    if(res) {
        printf("Failed to dump BOOT1_SLC.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_boot1_vwii_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Dumping BOOT1_SLCCMPT.RAW...\n");

    res = _dump_slc_raw(NAND_BANK_SLCCMPT, 1);
    if(res) {
        printf("Failed to dump BOOT1_SLCCMPT.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_slc_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring SLC.RAW...\n");

    res = _dump_restore_slc_raw(NAND_BANK_SLC, 0, false);
    if(res) {
        printf("Failed to restore SLC.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_test_slc_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Testing SLC and Restoring SLC.RAW...\n");

    res = _dump_restore_slc_raw(NAND_BANK_SLC, 0, true);
    if(res) {
        printf("Failed to restore SLC.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}
// does not work, as the HMACs would be wrong
void dump_restore_slc_img(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Testing SLC and Restoring SLC.IMG...\n");

    res = _dump_restore_slc_img(NAND_BANK_SLC, 0);
    if(res) {
        printf("Failed to restore SLC.IMG (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_slccmpt_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring SLCCMPT.RAW...\n");

    res = _dump_restore_slc_raw(NAND_BANK_SLCCMPT, 0, false);
    if(res) {
        printf("Failed to restore SLCCMPT.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_boot1_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring BOOT1_SLC.RAW...\n");

    res = _dump_restore_slc_raw(NAND_BANK_SLC, 1, false);
    if(res) {
        printf("Failed to restore BOOT1_SLC.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_boot1_vwii_raw(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring BOOT1_SLCCMPT.RAW...\n");

    res = _dump_restore_slc_raw(NAND_BANK_SLCCMPT, 1, false);
    if(res) {
        printf("Failed to restore BOOT1_SLCCMPT.RAW (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_boot1_img(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring BOOT1_SLC.IMG...\n");

    res = _dump_restore_slc_img(NAND_BANK_SLC, 1);
    if(res) {
        printf("Failed to restore BOOT1_SLC.IMG (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_restore_boot1_vwii_img(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring BOOT1_SLCCMPT.IMG...\n");

    res = _dump_restore_slc_img(NAND_BANK_SLCCMPT, 1);
    if(res) {
        printf("Failed to restore BOOT1_SLCCMPT.IMG (%d)!\n", res);
        goto slc_exit;
    }

slc_exit:
    console_power_to_exit();
}

void dump_format_rednand(void)
{
    int res = 0;

    gfx_clear(GFX_ALL, BLACK);
    printf("Formatting redNAND...\n");

    mbr_sector mbr ALIGNED(32) = {0};

    res = _dump_partition_rednand();
    if(res > 0) return;
    if(res < 0) goto format_exit;

    res = sdcard_read(0, 1, &mbr);
    if(res) {
        printf("Failed to read MBR (%d)!\n", res);
        goto format_exit;
    }

    smc_get_events(); // Eat all existing events
    printf("Dump SLC/SLCCMPT-RAW images? These are useful for sysNAND restore.\n");
    if(!console_abort_confirmation_power_skip_eject_dump())
    {
        printf("Dumping SLC-RAW to FAT32...\n");
        res = _dump_slc_raw(NAND_BANK_SLC, 0);
        if(res) {
            printf("Failed to dump SLC-RAW (%d)!\n", res);
            goto format_exit;
        }

        printf("Dumping SLCCMPT-RAW to FAT32...\n");
        res = _dump_slc_raw(NAND_BANK_SLCCMPT, 0);
        if(res) {
            printf("Failed to dump SLCCMPT-RAW (%d)!\n", res);
            goto format_exit;
        }
    }

    u32 mlc_base = LD_DWORD(mbr.partition[1].lba_start);
    u32 slc_base = LD_DWORD(mbr.partition[2].lba_start);
    u32 slccmpt_base = LD_DWORD(mbr.partition[3].lba_start);

    printf("Dumping redNAND...\n");
    res = _dump_copy_rednand(slc_base, slccmpt_base, mlc_base);
    if(res) {
        printf("Failed to dump redNAND (%d)!\n", res);
        goto format_exit;
    }

format_exit:
    console_power_to_exit();
}

void dump_erase_mlc(void){
    gfx_clear(GFX_ALL, BLACK);
    printf("Erase MLC\n");

    if(!isfs_slc_has_isfshax_installed() && !crypto_otp_is_de_Fused){
        printf("MLC Erase not allowed!\nNeither ISFShax nor defuse is detected\nMLC erase would brick the consolse.");
        goto erase_exit;
    }

    if (console_abort_confirmation_power_no_eject_yes()) 
        goto erase_exit;

    if(mlc_init())
        goto erase_exit;

    printf("Erasing...\n");

    int res = mlc_erase();
    if(res)
        printf("MLC erase failed\n");
    else
        printf("MLC erase complete!\n");

erase_exit:
    console_power_to_exit();

}

void dump_restore_rednand(void)
{
    gfx_clear(GFX_ALL, BLACK);
    printf("Restoring redNAND...\n");

    int res = rednand_load_mbr();
    if(res < 0 || !rednand.mlc.lba_length){
        printf("Failed to find redNAND MLC partition\n");
        goto restore_exit;
    }

    if(!isfs_slc_has_isfshax_installed() && !crypto_otp_is_de_Fused){
        printf("MLC restore not allowed!\nNeither ISFShax nor defuse is detected\nMLC restore would brick the consolse.");
        goto restore_exit;
    }

    smc_get_events(); // Eat all existing events

    printf("Restoring MLC...\n");
    res = _dump_restore_mlc(rednand.mlc.lba_start);
    if(res) {
        printf("Failed to restore MLC (%d)!\n", res);
        goto restore_exit;
    }

    // TODO: ask to restore SLC and SLCCMPT as well.

    printf("redNAND restore complete!\n");

restore_exit:
    clear_rednand();
    console_power_to_exit();
}

void dump_otp_via_prshhax(void)
{
    const u8 key_prod[16] = {0xB5, 0xD8, 0xAB, 0x06, 0xED, 0x7F, 0x6C, 0xFC, 0x52, 0x9F, 0x2C, 0xE1, 0xB4, 0xEA, 0x32, 0xFD};
    const u8 key_dev[16] = {0x2D, 0xC1, 0x9B, 0xDA, 0x70, 0x9C, 0x57, 0x21, 0xA8, 0x7E, 0x5C, 0x5F, 0x71, 0x43, 0xA2, 0x78};
    const u8 key_zero[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    void* payload_dst = (void*)PRSHHAX_PAYLOAD_DST;
    u32 boot_info_addr = 0x0;
    char* console_type_str = "unk";
    int has_mismatch = 0;
    ancast_header* hdr = (ancast_header*)(nand_page_buf + 0x1A0);
    u32 type_slot0 = 0;
    u32 version_slot0 = 0;
    u32 type_slot1 = 0;
    u32 version_slot1 = 0;

    nand_initialize(NAND_BANK_SLC);

    // TODO: technically these can be altered in SEEPROM, but also pls don't do that.
    // Read slot0 boot1 header
    nand_read_page(0, nand_page_buf, nand_ecc_buf);

    type_slot0 = hdr->type;
    version_slot0 = hdr->version & 0xFFFF;

    // Read slot1 boot1 header
    nand_read_page(BOOT1_MAX_PAGE, nand_page_buf, nand_ecc_buf);

    type_slot1 = hdr->type;
    version_slot1 = hdr->version & 0xFFFF;

    u32 boot1_type = version_slot1 > version_slot0 ? type_slot1 : type_slot0;
    u32 boot1_version = version_slot1 > version_slot0 ? version_slot1 : version_slot0;

    if (boot1_type == ANCAST_CONSOLE_TYPE_PROD) {
        console_type_str = "prod";
        switch(boot1_version)
        {
        case 8296:
            boot_info_addr = 0x0D40A7E5;
            break;
        case 8325:
            boot_info_addr = 0x0D40A7A5;
            break;
        case 8338:
            boot_info_addr = 0x0D40ABA5;
            break;
        case 8342:
            boot_info_addr = 0x0D40ABA9;
            break;
        case 8354:
            boot_info_addr = 0x0D40ABD5;
            break;
        case 8377:
            boot_info_addr = 0x0D40AC6D;
            break;
        default:
            printf("Unknown prod boot1 version: v%u (%04x).\n");
            printf("Either your NAND is corrupt of you've got something exotic,\n");
            printf("maybe ask ShinyQuagsire to add a bruteforce option.\n");
            goto fail;
        }
    }
    else if (boot1_type == ANCAST_CONSOLE_TYPE_DEV) {
        console_type_str = "dev";
        switch(boot1_version)
        {
        case 8296:
            boot_info_addr = 0x0D40A78D;
            break;
        case 8297:
        case 8325:
            boot_info_addr = 0x0D40A7A5;
            break;
        case 8339:
            boot_info_addr = 0x0D40AB69;
            break;
        case 8342:
            boot_info_addr = 0x0D40ABA9;
            break;
        case 8354:
            boot_info_addr = 0x0D40ABD5;
            break;
        case 8377:
            boot_info_addr = 0x0D40AC6D;
            break;
        case 8378:
            boot_info_addr = 0x0D40AC91;
            break;
        default:
            printf("Unknown dev boot1 version: v%u (%04x).\n");
            printf("Either your NAND is corrupt of you've got something exotic,\n");
            printf("maybe ask ShinyQuagsire to add a bruteforce option.\n");
            goto fail;
        }
    }
    else {
        printf("boot1 might be corrupt? Console type: %02x\n");
        printf("Can't continue.\n");
        goto fail;
    }

    prsh_set_entry("boot_info", (void*)(boot_info_addr), 0x58);

    if (!memcmp(otp.fw_ancast_key, key_zero, 16)) {
        u8 hwver = latte_get_hw_version() & 0xFF;
        
        /*printf("Guessing key based on hwver %02x == %02x\n", hwver, BSP_HARDWARE_VERSION_CAFE);
        if (!hwver || hwver == BSP_HARDWARE_VERSION_CAFE) {
            printf("  --> prod key\n");
            memcpy(otp.fw_ancast_key, key_prod, 16);
        }
        else {
            printf("  --> dev key\n");
            memcpy(otp.fw_ancast_key, key_dev, 16);
        }*/

        printf("Guessing key based on boot1 header type %x\n", hdr->type);
        if (boot1_type == ANCAST_CONSOLE_TYPE_DEV) {
            printf("  --> dev key\n");
            memcpy(otp.fw_ancast_key, key_dev, 16);
        }
        else {
            printf("  --> prod key\n");
            memcpy(otp.fw_ancast_key, key_prod, 16);
        }
    }
    otp.security_level |= 0x80000000;

    printf("Dumping OTP using boot1 %s v%u (slot0=v%u, slot1=v%u), and offset 0x%08x...\n", console_type_str, boot1_version, version_slot0, version_slot1, boot_info_addr);

    if (seeprom_decrypted.boot1_params.version != version_slot0) {
        has_mismatch = 1;
        printf("\nWARNING: SEEPROM slot0 boot1 version v%u does not match NAND version v%u!\n", seeprom_decrypted.boot1_params.version, version_slot0);
        printf("         Exploit might not work!\n\n");
    }
    if (seeprom_decrypted.boot1_copy_params.version != version_slot1) {
        has_mismatch = 1;
        printf("\nWARNING: SEEPROM slot1 boot1 version v%u does not match NAND version v%u!\n", seeprom_decrypted.boot1_copy_params.version, version_slot1);
        printf("         Exploit might not work!\n\n");
    }

    if (has_mismatch) {
        printf("If this is the first time you're dumping otp.bin, ignore this message.\n");
        printf("However, if you reflashed boot1, you might have to guess which boot1\n");
        printf("version was originally on NAND and will match the SEEPROM version.\n");
    }

    prsh_print();
    prsh_encrypt();
    write32(0x0, 0xEA000010); // b 0x48
    memcpy(payload_dst, boot1_prshhax_payload, ((u32)boot1_prshhax_payload_end-(u32)boot1_prshhax_payload));

    dc_flushrange(payload_dst, 0x1000);

    write32(PRSHHAX_OTPDUMP_PTR, PRSHHAX_FAIL_MAGIC);
    dc_flushrange((void*)PRSHHAX_OTPDUMP_PTR, 0x20);

    main_reset_no_defuse();
    return;

fail:
    console_power_to_exit();
}

static void _dump_delete(const char* path){
    printf("Delete %s\n", path);

    if (console_abort_confirmation_power_no_eject_yes()) 
        return;

    printf("Deleting...\n");

    int res = unlink(path);
    if(res)
        printf("Delete failed: %i\n", res);
    else
        printf("Delete complete!\n");

    console_power_to_exit();

}

static void _dump_delete_scfm(void){
    gfx_clear(GFX_ALL, BLACK);

    if(!isfs_slc_has_isfshax_installed() && !crypto_otp_is_de_Fused){
        printf("SCFM delete not allowed!\nNeither ISFShax nor defuse is detected\nSCFM delete would brick the consolse.");
        console_power_to_continue();
        return;
    }

    if(isfs_init(ISFSVOL_SLC)<0){
            console_power_to_continue();
            return;
    }

    _dump_delete("slc:/scfm.img");
}

static void _dump_delete_scfm_slccmpt(void){
    gfx_clear(GFX_ALL, BLACK);
    if(isfs_init(ISFSVOL_SLCCMPT)<0){
        console_power_to_continue();
        return;
    }
    _dump_delete("slccmpt:/scfm.img");
}

static void _dump_delete_scfm_rednand(void){
    gfx_clear(GFX_ALL, BLACK);
    int error = init_rednand();
    if(error<0){
        console_power_to_continue();
        return;
    }
    if(!rednand.slc.lba_length){
        printf("redslc not configured\n");
        console_power_to_continue();
        return;
    }

    if(isfs_init(ISFSVOL_REDSLC)<0){
        console_power_to_continue();
        return;
    }
    _dump_delete("redslc:/scfm.img");
}

int copy_file(const char* from, const char* to){
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

static void _copy_dir(const char* dir, const char* dest){
    printf("Dumping %s\n", dir);

    int res = mkdir(dest, 777);
    if(res){
        if(errno != EEXIST) {
            printf("ERROR creating %s: %i\n", dest, errno);
            console_power_to_continue();
            return;
        }
        printf("Warning: '%s' directory already exists\n", dest);
        if (console_abort_confirmation_power_no_eject_yes()) 
            return;
    }
    DIR *dfd = opendir(dir);
    if(!dfd){
        printf("ERROR opening %s: %i\n", dir, errno);
        console_power_to_continue();
        return;
    }
    struct dirent *dp;
    while(dp = readdir(dfd)){
        char src_pathbuf[255];
        snprintf(src_pathbuf, 254, "%s/%s", dir, dp->d_name);
        char dst_pathbuf[255];
        snprintf(dst_pathbuf, 254, "%s/%s", dest, dp->d_name);

        printf("Dumping %s\n", src_pathbuf);
        int res = copy_file(src_pathbuf, dst_pathbuf);
        if(res){
            printf("Error copying %s\n", src_pathbuf);
        }
    } 

    closedir(dfd);

}

void dump_logs_slc(void){
    gfx_clear(GFX_ALL, BLACK);
    if(isfs_init(ISFSVOL_SLC)<0){
        console_power_to_continue();
        return;
    }
    _copy_dir("slc:/sys/logs", "sdmc:/logs");
    console_power_or_eject_to_return();
}

void dump_logs_redslc(void){
    gfx_clear(GFX_ALL, BLACK);
    int error = init_rednand();
    if(error<0){
        console_power_to_continue();
        return;
    }
    if(!rednand.slc.lba_length){
        printf("redslc not configured\n");
        console_power_to_continue();
        return;
    }

    if(isfs_init(ISFSVOL_REDSLC)<0){
        console_power_to_continue();
        return;
    }
    _copy_dir("redslc:/sys/logs", "sdmc:/redlogs");
    console_power_or_eject_to_return();
}

#endif // FASTBOOT
#endif // MINUTE_BOOT1
