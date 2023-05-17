/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "crypto.h"
#include "latte.h"
#include "utils.h"
#include "memory.h"
#include "irq.h"
#include "gfx.h"
#include "string.h"
#include "seeprom.h"
#include "crc32.h"
#include "serial.h"

#define     AES_CMD_RESET   0
#define     AES_CMD_DECRYPT 0x9800
#define     AES_CMD_ENCRYPT 0x9000
#define     AES_CMD_COPY    0x8000

otp_t otp;
seeprom_t seeprom;
seeprom_t seeprom_decrypted;
int crypto_otp_is_de_Fused = 0;

bc_t default_bc =
{
    4, 0x404D, 0x4346, 0xB, 0x4E31, 0x800, 2, 5, 2, 1, 0x5521, 0, 0xF8, 3, 1, 0, {0}
};

void crypto_read_otp(void)
{
    u32 *otpd = (u32*)&otp;
    int word, bank;
    for (bank = 0; bank < 8; bank++)
    {
        for (word = 0; word < 0x20; word++)
        {
            write32(LT_OTPCMD, 0x80000000 | bank << 8 | word);
            *otpd++ = read32(LT_OTPDATA);
        }
    }
}

void crypto_read_seeprom(void)
{
    seeprom_read(&seeprom, 0, sizeof(seeprom) / 2);

    // Added: fallback
    if (!seeprom.bc_size || seeprom.bc_size != 0x24 || crc32(&seeprom.bc_size, seeprom.bc_size - sizeof(u32)) != seeprom.bc_crc32)
    {
#ifdef MINUTE_BOOT1
        serial_send_u32(0x46414C4C);
#endif
        memcpy(&seeprom.bc, &default_bc, sizeof(seeprom.bc));
        seeprom.bc_size = 0x24;

        for (int i = 0; i < 0x20; i++)
        {
            if (i && i % 0x10 == 0) {
                printf("\n");
            }
            printf("%02x ", *(u8*)((u32)&seeprom.bc + i));
        }
        printf("\n");
    }
}

int crypto_check_de_Fused()
{
    if (crypto_otp_is_de_Fused) {
        return crypto_otp_is_de_Fused;
    }

    int has_jtag = 0;
    int bytes_loaded = 0x3FF;

    crypto_otp_is_de_Fused = 0;
    u8* otp_iter = ((u8*)&otp) + sizeof(otp) - 5;
    while (!(*otp_iter))
    {
        otp_iter--;
        if (--bytes_loaded <= 0) {
            break;
        }
    }

    if (!otp.jtag_status) {
        has_jtag = 1;
    }

    printf("crypto: ~0x%03x bytes of OTP loaded; JTAG is %s (%08x)\n", bytes_loaded, has_jtag ? "enabled" : "disabled", otp.jtag_status);

    otp_iter = ((u8*)&otp);
    for (int i = 0; i < bytes_loaded; i++)
    {
        if (i && i % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", *otp_iter++);
    }
    printf("\n");

    if (bytes_loaded <= 0x90) {
        crypto_otp_is_de_Fused = 1;
    }
    return crypto_otp_is_de_Fused;
}

void crypto_initialize(void)
{
    crypto_read_otp();
    crypto_read_seeprom();

    aes_reset();
    irq_enable(IRQ_AES);

    memcpy(&seeprom_decrypted, &seeprom, sizeof(seeprom));
}

void crypto_decrypt_seeprom()
{
    crypto_decrypt_verify_seeprom_ptr(&seeprom_decrypted, &seeprom);
}

int crypto_verify_seeprom_ptr(seeprom_t* pSeeprom)
{
    // Verify non-crypted parameters
    if (pSeeprom->AA55_marker != 0xAA55) {
        printf("seeprom: bad AA55 marker! %04x\n", pSeeprom->AA55_marker);
        return 0;
    }
    if (pSeeprom->BB66_marker != 0xBB66) {
        printf("seeprom: bad BB66 marker! %04x\n", pSeeprom->BB66_marker);
        return 0;
    }
    u32 crc_check = crc32(&pSeeprom->production_timestamp, sizeof(pSeeprom->production_timestamp));
    if (crc_check != pSeeprom->production_timestamp_crc32) {
        printf("seeprom: bad production CRC32! calc=%08x, stored=%08x\n", crc_check, pSeeprom->production_timestamp_crc32);
        return 0;
    }
    if (pSeeprom->bc_size != 0x24) {
        printf("seeprom: bad bc_size! %04x\n", pSeeprom->bc_size);
        return 0;
    }
    crc_check = crc32(&pSeeprom->bc_size, pSeeprom->bc_size - sizeof(u32));
    if (crc_check != pSeeprom->bc_crc32) {
        printf("seeprom: bad production CRC32! calc=%08x, stored=%08x\n", crc_check, pSeeprom->bc_crc32);
        return 0;
    }

    return 1;
}

int crypto_decrypt_verify_seeprom_ptr(seeprom_t* pOut, seeprom_t* pSeeprom)
{
    static u8 seeprom_tmp[0x10] ALIGNED(16);

    memcpy(pOut, pSeeprom, sizeof(*pSeeprom));
    aes_reset();
    aes_set_key(otp.seeprom_key);

    // Decrypt the three SEEPROM blocks
    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->hw_params, 0x10);
    aes_decrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->hw_params, seeprom_tmp, 0x10);

    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->boot1_params, 0x10);
    aes_decrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->boot1_params, seeprom_tmp, 0x10);

    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->boot1_copy_params, 0x10);
    aes_decrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->boot1_copy_params, seeprom_tmp, 0x10);

    u32 crc_1 = crc32(&pOut->hw_params, 0xC);
    u32 crc_2 = crc32(&pOut->boot1_params, 0xC);
    u32 crc_3 = crc32(&pOut->boot1_copy_params, 0xC);

    if (crc_1 == pOut->hw_params_crc32 && crc_2 == pOut->boot1_params_crc32 && crc_3 == pOut->boot1_copy_params_crc32 && crypto_verify_seeprom_ptr(pOut)) {
        return 1;
    }

    printf("SEEPROM failed to verify!\n");
    printf("(Check your otp.bin?)\n");
    printf("Hardware params         calc: %08x stored: %08x\n", crc_1, pOut->hw_params_crc32);
    printf("Primary boot1 params    calc: %08x stored: %08x\n", crc_2, pOut->boot1_params);
    printf("Secondary boot1 params  calc: %08x stored: %08x\n", crc_3, pOut->boot1_copy_params);
    printf("Decrypted boot1 versions: v%u (%04x) and v%u (%04x)\n", pOut->boot1_params.version, pOut->boot1_params.version, pOut->boot1_copy_params.version, pOut->boot1_copy_params.version);
    printf("Decrypted boot1 sectors: 0x%04x and 0x%04x\n", pOut->boot1_params.sector, pOut->boot1_copy_params.sector);
    return 0;
}

int crypto_encrypt_verify_seeprom_ptr(seeprom_t* pOut, seeprom_t* pSeeprom)
{
    seeprom_t extra_verify;
    static u8 seeprom_tmp[0x10] ALIGNED(16);

    memcpy(pOut, pSeeprom, sizeof(*pSeeprom));

    u32 crc_1 = crc32(&pOut->hw_params, 0xC);
    u32 crc_2 = crc32(&pOut->boot1_params, 0xC);
    u32 crc_3 = crc32(&pOut->boot1_copy_params, 0xC);

    if (crc_1 != pOut->hw_params_crc32 || crc_2 != pOut->boot1_params_crc32 || crc_3 != pOut->boot1_copy_params_crc32 || !crypto_verify_seeprom_ptr(pOut)) {
        printf("SEEPROM failed to verify!\n");
        printf("(Check your otp.bin?)\n");
        printf("Hardware params         calc: %08x stored: %08x\n", crc_1, pOut->hw_params_crc32);
        printf("Primary boot1 params    calc: %08x stored: %08x\n", crc_2, pOut->boot1_params);
        printf("Secondary boot1 params  calc: %08x stored: %08x\n", crc_3, pOut->boot1_copy_params);
        printf("Decrypted boot1 versions: v%u (%04x) and v%u (%04x)\n", pOut->boot1_params.version, pOut->boot1_params.version, pOut->boot1_copy_params.version, pOut->boot1_copy_params.version);
        printf("Decrypted boot1 sectors: 0x%04x and 0x%04x\n", pOut->boot1_params.sector, pOut->boot1_copy_params.sector);

        return 0;
    }

    aes_reset();
    aes_set_key(otp.seeprom_key);

    // Decrypt the three SEEPROM blocks
    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->hw_params, 0x10);
    aes_encrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->hw_params, seeprom_tmp, 0x10);

    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->boot1_params, 0x10);
    aes_encrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->boot1_params, seeprom_tmp, 0x10);

    aes_empty_iv();
    memcpy(seeprom_tmp, &pOut->boot1_copy_params, 0x10);
    aes_encrypt(seeprom_tmp, seeprom_tmp, 1, 0);
    memcpy(&pOut->boot1_copy_params, seeprom_tmp, 0x10);

    // Juuust in case
    return crypto_decrypt_verify_seeprom_ptr(&extra_verify, pOut);
}

static int _aes_irq = 0;

void aes_irq(void)
{
    _aes_irq = 1;
}

static inline void aes_command(u16 cmd, u8 iv_keep, u32 blocks)
{
    if (blocks != 0)
        blocks--;
    _aes_irq = 0;
    write32(AES_CTRL, (cmd << 16) | (iv_keep ? 0x1000 : 0) | (blocks&0x7f));
    while (read32(AES_CTRL) & 0x80000000);
}

void aes_reset(void)
{
    write32(AES_CTRL, 0);
    while (read32(AES_CTRL) != 0);
}

void aes_set_iv(u8 *iv)
{
    u32 iv_tmp[4];
    memcpy(iv_tmp, iv, 4*sizeof(u32));

    for(int i = 0; i < 4; i++) {
        write32(AES_IV, iv_tmp[i]);
    }
}

void aes_empty_iv(void)
{
    for(int i = 0; i < 4; i++) {
        write32(AES_IV, 0);
    }
}

void aes_set_key(u8 *key)
{
    u32 key_tmp[4];
    memcpy(key_tmp, key, 4*sizeof(u32));

    for(int i = 0; i < 4; i++) {
        write32(AES_KEY, key_tmp[i]);
    }
}

void aes_decrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv)
{
    // Kinda have to do both flush/invalidate on both because if you crypt
    // 1 block, an invalidate will corrupt the periphery memory in the cache
    // line.
    dc_flushrange(src, blocks * 16);
    dc_invalidaterange(src, blocks * 16);
    dc_flushrange(dst, blocks * 16);
    dc_invalidaterange(dst, blocks * 16);
    ahb_flush_to(RB_AES);

    int this_blocks = 0;
    while(blocks > 0) {
        this_blocks = blocks;
        if (this_blocks > 0x80)
            this_blocks = 0x80;

        write32(AES_SRC, dma_addr(src));
        write32(AES_DEST, dma_addr(dst));

        aes_command(AES_CMD_DECRYPT, keep_iv, this_blocks);

        blocks -= this_blocks;
        src += this_blocks<<4;
        dst += this_blocks<<4;
        keep_iv = 1;
    }

    ahb_flush_from(WB_AES);
    ahb_flush_to(RB_IOD);
    //dc_flushrange(dst, blocks * 16);
    //dc_invalidaterange(dst, blocks * 16);
}

void aes_encrypt(u8 *src, u8 *dst, u32 blocks, u8 keep_iv)
{
    // Kinda have to do both flush/invalidate on both because if you crypt
    // 1 block, an invalidate will corrupt the periphery memory in the cache
    // line.
    dc_flushrange(src, blocks * 16);
    dc_invalidaterange(src, blocks * 16);
    dc_flushrange(dst, blocks * 16);
    dc_invalidaterange(dst, blocks * 16);
    ahb_flush_to(RB_AES);

    int this_blocks = 0;
    while(blocks > 0) {
        this_blocks = blocks;
        if (this_blocks > 0x80)
            this_blocks = 0x80;

        write32(AES_SRC, dma_addr(src));
        write32(AES_DEST, dma_addr(dst));

        aes_command(AES_CMD_ENCRYPT, keep_iv, this_blocks);

        blocks -= this_blocks;
        src += this_blocks<<4;
        dst += this_blocks<<4;
        keep_iv = 1;
    }

    ahb_flush_from(WB_AES);
    ahb_flush_to(RB_IOD);
    //dc_flushrange(dst, blocks * 16);
    //dc_invalidaterange(dst, blocks * 16);
}

void aes_copy(u8 *src, u8 *dst, u32 blocks)
{
    // Kinda have to do both flush/invalidate on both because if you crypt
    // 1 block, an invalidate will corrupt the periphery memory in the cache
    // line.
    dc_flushrange(src, blocks * 16);
    dc_invalidaterange(src, blocks * 16);
    dc_flushrange(dst, blocks * 16);
    dc_invalidaterange(dst, blocks * 16);
    ahb_flush_to(RB_AES);

    int this_blocks = 0;
    while(blocks > 0) {
        this_blocks = blocks;
        if (this_blocks > 0xFFF)
            this_blocks = 0xFFF;

        write32(AES_SRC, dma_addr(src));
        write32(AES_DEST, dma_addr(dst));

        aes_command(AES_CMD_COPY, false, this_blocks);

        blocks -= this_blocks;
        src += this_blocks<<4;
        dst += this_blocks<<4;
    }

    ahb_flush_from(WB_AES);
    ahb_flush_to(RB_IOD);
    //dc_flushrange(dst, blocks * 16);
    //dc_invalidaterange(dst, blocks * 16);
}
