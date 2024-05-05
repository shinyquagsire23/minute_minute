/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "types.h"

#define SATA_TYPE_DEFAULT 1
#define SATA_TYPE_NODRIVE 2
#define SATA_TYPE_ROMDRIVE 3
#define SATA_TYPE_RDRIVE 4
#define SATA_TYPE_MION 5
#define SATA_TYPE_SES 6
#define SATA_TYPE_GEN2HDD 7
#define SATA_TYPE_GEN1HDD 8

typedef struct
{
    // Bank 0 (Wii)
    struct {
        u8 wii_boot1_hash[20];
        u8 wii_common_key[16];
        u32 wii_ng_id;
        union {
             struct {
                 u8 wii_ng_priv[30];
                 u8 _wii_wtf1[18];
             };
             struct {
                 u8 _wii_wtf2[28];
                 u8 wii_nand_hmac[20];
             };
        };
        u8 wii_nand_key[16];
        u8 wii_rng_key[16];
        u32 wii_unk1;
        u32 wii_unk2; // 0x00000007
    };

    // Bank 1 (Wii U)
    struct {
        u32 security_level;
        u32 iostrength_flag;
        u32 seeprom_pulse;
        u32 unk1;
        u8 fw_ancast_key[16];
        u8 seeprom_key[16];
        u8 unk2[16];
        u8 unk3[16];
        u8 vwii_common_key[16];
        u8 common_key[16];
        u8 unk4[16];
    };

    // Bank 2 (Wii U)
    struct {
        u8 unk5[16];
        u8 unk6[16];
        u8 ssl_master_key[16];
        u8 external_master_key[16];
        u8 unk7[16];
        u8 xor_key[16];
        u8 rng_key[16];
        u8 nand_key[16];
    };

    // Bank 3 (Wii U)
    struct {
        u8 emmc_key[16];
        u8 dev_master_key[16];
        u8 drh_key[16];
        u8 unk8[48];
        u8 nand_hmac[20];
        u8 unk9[12];
    };

    // Bank 4 (Wii U)
    struct {
        u8 unk10[16];
        union {
            u8 external_seed_full[16];
            struct {
                u8 _wtf1[12];
                u8 external_seed[4];
            };
        };
        u8 vwii_ng_priv[32];
        u8 unk11[32];
        union {
            u8 rng_seed_full[16];
            struct {
                u32 rng_seed;
                u8 _wtf2[12];
            };
        };
        u8 unk12[16];
    };

    // Bank 5 (Wii U)
    struct {
        u32 rootca_version;
        u32 rootca_ms;
        u32 unk13;
        u8 rootca_signature[64];
        u8 unk14[20];
        u8 unk15[32];
    };

    // Bank 6 (Wii SEEPROM)
    struct {
        u8 wii_seeprom_certificate[96];
        u8 wii_seeprom_signature[32];
    };

    // Bank 7 (Misc.)
    struct {
        u8 unk16[32];
        u8 boot1_key[16];
        u8 unk17[16];
        u8 _empty1[32];
        u8 unk18[16];
        char ascii_tag[12];
        u32 jtag_status;
    };
} __attribute__((packed)) ALIGNED(4) otp_t;

_Static_assert(sizeof(otp_t) == 0x400, "OTP size must be 0x400!");

typedef struct __attribute__((packed)) {
    u16 library_version;
    u16 author;
    u16 board_type;
    u16 board_revision;
    u16 boot_source;
    u16 ddr3_size;
    u16 ddr3_speed;
    u16 ppc_clock_multiplier;
    u16 iop_clock_multiplier;
    u16 video_1080p;
    u16 ddr3_vendor;
    u16 mov_passive_reset;
    u16 sys_pll_speed;
    u16 sata_device;
    u16 console_type;
    u32 device_presence;
    u8 reserved[32];
} bc_t;

typedef struct
{
    u8 _empty1[18];
    u64 rng_seed;
    u8 _empty2[6];
    u32 ppc_pvr;
    char ascii_tag1[6];
    u8 unk1[6];
    u8 otp_tag[8];

    // BoardConfig
    u32 bc_crc32;
    u16 bc_size;
    bc_t bc;

    u8 drive_key[16];
    u8 factory_key[16];
    u8 dev_key[16];
    u8 external_key[16];
    u16 drive_key_status;
    u16 external_key_status;
    u16 dev_key_status;
    u8 _empty3[106];
    u32 unk2;
    u32 unk3;
    u8 unk4[8];

    struct __attribute__((packed)) {
        u32 version;
        u32 eeprom_version;
        u32 product_area;
        u32 game_region;
        u32 ntsc_pal;
        u16 country_code;
        u16 country_code_revision;
        u8 code_id[8];
        u8 serial_id[16];
        u8 model_number[16];
    } sys_prod;

    struct __attribute__((packed)) {
        u32 unk5;
        u8 unk6[4];
        u16 production_year;
        u16 production_date;
        u16 production_time;
    } production_timestamp;
    u32 production_timestamp_crc32;

    u16 AA55_marker;
    u8 unk7[6];
    u8 _empty[4];
    u8 unk8[4];
    u16 BB66_marker;
    u8 unk9[4];

    char ascii_tag2[8];

    u8 unk10[16];

    struct __attribute__((packed)) {
        u16 clock_control;
        u16 nand_control;
        u32 nand_config;
        u32 nand_bank;
    } hw_params;
    u32 hw_params_crc32;

    struct __attribute__((packed)) {
        u16 version;
        u16 sector;
        u8 empty[8];
    } boot1_params;
    u32 boot1_params_crc32;

    struct __attribute__((packed)) {
        u16 version;
        u16 sector;
        u8 empty[8];
    } boot1_copy_params;
    u32 boot1_copy_params_crc32;

    u8 _empty4[16];
} __attribute__((packed)) seeprom_t;

_Static_assert(sizeof(seeprom_t) == 0x200, "SEEPROM size must be 0x200!");

extern otp_t otp;
extern seeprom_t seeprom;
extern seeprom_t seeprom_decrypted;
extern int crypto_otp_is_de_Fused;

void crypto_read_otp();
int crypto_check_de_Fused();

void crypto_initialize();
void crypto_read_seeprom(void);
void crypto_decrypt_seeprom(void);
int crypto_decrypt_verify_seeprom_ptr(seeprom_t* pOut, seeprom_t* pSeeprom);
int crypto_encrypt_verify_seeprom_ptr(seeprom_t* pOut, seeprom_t* pSeeprom);

void aes_reset(void);
void aes_set_iv(u8 *iv);
void aes_empty_iv();
void aes_set_key(u8 *key);
void aes_decrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv);
void aes_encrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv);
void aes_copy(u8 *src, u8 *dst, u32 blocks);

#endif

