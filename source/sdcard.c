/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "bsdtypes.h"
#include "sdhc.h"
#include "sdcard.h"
#include "gfx.h"
#include "string.h"
#include "utils.h"
#include "memory.h"
#include "gpio.h"
#include "elm.h"

#include "latte.h"

#ifdef CAN_HAZ_IRQ
#include "irq.h"
#endif

extern bool elm_mounted;
//#define SDCARD_DEBUG

#ifdef SDCARD_DEBUG
static int sdcarddebug = 2;
#define DPRINTF(n,s)    do { if ((n) <= sdcarddebug) printf s; } while (0)
#else
#define DPRINTF(n,s)    do {} while(0)
#endif

static struct sdhc_host sdcard_host;

struct sdcard_ctx {
    sdmmc_chipset_handle_t handle;
    int inserted;
    int sdhc_blockmode;
    int selected;
    int new_card; // set to 1 everytime a new card is inserted
    int multiple_fallback;

    u32 num_sectors;
    u16 rca;
};

static struct sdcard_ctx card;

void sdcard_attach(sdmmc_chipset_handle_t handle)
{
#ifndef MINUTE_BOOT1
    //bool should_remount = elm_mounted;
    ELM_Unmount();
#endif

    memset(&card, 0, sizeof(card));

    card.handle = handle;

    DPRINTF(0, ("sdcard: attached new SD/MMC card\n"));

    sdhc_host_reset(card.handle);

    if (sdhc_card_detect(card.handle)) {
        DPRINTF(1, ("card is inserted. starting init sequence.\n"));

        for (int i = 0; i < 16; i++)
        {
            sdcard_needs_discover();
            if (card.inserted) break;
        }
#ifndef MINUTE_BOOT1
        //if (should_remount) {
        ELM_Mount();
        //}
#endif
    }
}

void sdcard_abort(void) {
    struct sdmmc_command cmd;
    printf("sdcard: abortion kthx\n");

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_STOP_TRANSMISSION;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
}

void sdcard_needs_discover(void)
{
    struct sdmmc_command cmd;
    u32 ocr = card.handle->ocr;

    DPRINTF(0, ("sdcard: card needs discovery.\n"));
    sdhc_host_reset(card.handle);
    card.new_card = 1;

    if (!sdhc_card_detect(card.handle)) {
        DPRINTF(1, ("sdcard: card (no longer?) inserted.\n"));
        card.inserted = 0;
        return;
    }

    DPRINTF(1, ("sdcard: enabling power\n"));
    if (sdhc_bus_power(card.handle, ocr) != 0) {
        printf("sdcard: powerup failed for card\n");
        goto out;
    }

    DPRINTF(1, ("sdcard: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_400KHZ, SDMMC_TIMING_LEGACY) != 0) {
        printf("sdcard: could not enable clock for card\n");
        goto out_power;
    }

    sdhc_bus_width(card.handle, 1);

    DPRINTF(1, ("sdcard: sending GO_IDLE_STATE\n"));

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_GO_IDLE_STATE;
    cmd.c_flags = SCF_RSP_R0;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error) {
        printf("sdcard: GO_IDLE_STATE failed with %d\n", cmd.c_error);
        goto out_clock;
    }
    DPRINTF(2, ("sdcard: GO_IDLE_STATE response: %x\n", MMC_R1(cmd.c_resp)));

    DPRINTF(1, ("sdcard: sending SEND_IF_COND\n"));

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SEND_IF_COND;
    cmd.c_arg = 0x1aa;
    cmd.c_flags = SCF_RSP_R7;
    cmd.c_timeout = 100;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error || (cmd.c_resp[0] & 0xff) != 0xaa)
        ocr &= ~SD_OCR_SDHC_CAP;
    else
        ocr |= SD_OCR_SDHC_CAP;
    DPRINTF(2, ("sdcard: SEND_IF_COND ocr: %x\n", ocr));

    // TODO remove?
    card.sdhc_blockmode = 1;
    card.selected = 0;
    card.inserted = 1;
    card.multiple_fallback = 0;

    int tries;
    for (tries = 100; tries > 0; tries--) {
        udelay(100000);

        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_APP_CMD;
        cmd.c_arg = 0;
        cmd.c_flags = SCF_RSP_R1;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("sdcard: MMC_APP_CMD failed with %d\n", cmd.c_error);
            goto out_clock;
        }

        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = SD_APP_OP_COND;
        cmd.c_arg = ocr;
        cmd.c_flags = SCF_RSP_R3;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("sdcard: SD_APP_OP_COND failed with %d\n", cmd.c_error);
            goto out_clock;
        }

        DPRINTF(3, ("sdcard: response for SEND_IF_COND: %08x\n",
                    MMC_R1(cmd.c_resp)));
        if (ISSET(MMC_R1(cmd.c_resp), MMC_OCR_MEM_READY))
            break;
    }
    if (!ISSET(cmd.c_resp[0], MMC_OCR_MEM_READY)) {
        printf("sdcard: card failed to powerup.\n");
        goto out_power;
    }

    if (ISSET(MMC_R1(cmd.c_resp), SD_OCR_SDHC_CAP))
        card.sdhc_blockmode = 1;
    else
        card.sdhc_blockmode = 0;
    DPRINTF(2, ("sdcard: SDHC: %d\n", card.sdhc_blockmode));

    u8 *resp;
    u32 *resp32;

    DPRINTF(2, ("sdcard: MMC_ALL_SEND_CID\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_ALL_SEND_CID;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R2;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: MMC_ALL_SEND_CID failed with %d\n", cmd.c_error);
        goto out_clock;
    }

    resp = (u8 *)cmd.c_resp;
    resp32 = (u32 *)cmd.c_resp;
    printf("CID: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);
    printf("CID: mid=%02x name='%c%c%c%c%c%c%c' prv=%d.%d psn=%02x%02x%02x%02x mdt=%d/%d\n", resp[14],
        resp[13],resp[12],resp[11],resp[10],resp[9],resp[8],resp[7], resp[6], resp[5] >> 4, resp[5] & 0xf,
        resp[4], resp[3], resp[2], resp[0] & 0xf, 2000 + (resp[0] >> 4));

    DPRINTF(2, ("sdcard: SD_SEND_RELATIVE_ADDRESS\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R6;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: SD_SEND_RCA failed with %d\n", cmd.c_error);
        goto out_clock;
    }

    card.rca = MMC_R1(cmd.c_resp)>>16;
    DPRINTF(2, ("sdcard: rca: %08x\n", card.rca));

    card.selected = 0;
    card.inserted = 1;

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SEND_CSD;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R2;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: MMC_SEND_CSD failed with %d\n", cmd.c_error);
        goto out_power;
    }

    u8 csd_bytes[16];
    resp = (u8 *)cmd.c_resp;
    resp32 = (u32 *)cmd.c_resp;
    memcpy(csd_bytes, resp, 16);
    printf("CSD: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);

    if (csd_bytes[13] == 0xe) { // sdhc
        unsigned int c_size = csd_bytes[7] << 16 | csd_bytes[6] << 8 | csd_bytes[5];
        printf("sdcard: sdhc mode, c_size=%u, card size = %uk\n", c_size, (c_size + 1)* 512);
        card.num_sectors = (c_size + 1) * 1024; // number of 512-byte sectors
    }
    else {
        unsigned int taac, nsac, read_bl_len, c_size, c_size_mult;
        taac = csd_bytes[13];
        nsac = csd_bytes[12];
        read_bl_len = csd_bytes[9] & 0xF;

        c_size = (csd_bytes[8] & 3) << 10;
        c_size |= (csd_bytes[7] << 2);
        c_size |= (csd_bytes[6] >> 6);
        c_size_mult = (csd_bytes[5] & 3) << 1;
        c_size_mult |= csd_bytes[4] >> 7;
        printf("taac=%u nsac=%u read_bl_len=%u c_size=%u c_size_mult=%u card size=%u bytes\n",
            taac, nsac, read_bl_len, c_size, c_size_mult, (c_size + 1) * (4 << c_size_mult) * (1 << read_bl_len));
        card.num_sectors = (c_size + 1) * (4 << c_size_mult) * (1 << read_bl_len) / 512;
    }

    sdcard_select();

    DPRINTF(2, ("sdcard: MMC_SEND_STATUS\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SEND_STATUS;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: MMC_SEND_STATUS failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(2, ("sdcard: MMC_SET_BLOCKLEN\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SET_BLOCKLEN;
    cmd.c_arg = SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: MMC_SET_BLOCKLEN failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(2, ("sdcard: MMC_APP_CMD\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_APP_CMD;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: MMC_APP_CMD failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(2, ("sdcard: SD_APP_SET_BUS_WIDTH\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_APP_SET_BUS_WIDTH;
    cmd.c_arg = SD_ARG_BUS_WIDTH_4;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("sdcard: SD_APP_SET_BUS_WIDTH failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    sdhc_bus_width(card.handle, 4);

#ifndef MINUTE_BOOT1
    printf("sdcard: enabling highspeed 52MHz clock (%02x)\n", csd_bytes[0xB]);
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_52MHZ, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }

    printf("sdcard: could not enable highspeed clock for card, falling back to 48MHz highspeed?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_48MHZ, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }

    printf("sdcard: could not enable highspeed clock for card, falling back to 25MHz highspeed?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_25MHZ, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }
#endif

    printf("sdcard: could not enable highspeed clock for card, falling back to 25MHz legacy?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_25MHZ, SDMMC_TIMING_LEGACY) == 0) {
        return;
    }

    printf("sdcard: could not enable legacy clock for card, falling back to 400kHz?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_400KHZ, SDMMC_TIMING_LEGACY) == 0) {
        return;
    }

    printf("sdcard: no clocks? abort\n");
    goto out_clock;

out_clock:
    sdhc_bus_width(card.handle, 1);
    sdhc_bus_clock(card.handle, SDMMC_SDCLK_OFF, SDMMC_TIMING_LEGACY);

out_power:
    sdhc_bus_power(card.handle, 0);
out:
    return;
}


int sdcard_select(void)
{
    struct sdmmc_command cmd;

    DPRINTF(2, ("sdcard: MMC_SELECT_CARD\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SELECT_CARD;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
    printf("%s: resp=%x\n", __FUNCTION__, MMC_R1(cmd.c_resp));
//  sdhc_dump_regs(card.handle);

//  printf("present state = %x\n", HREAD4(hp, SDHC_PRESENT_STATE));
    if (cmd.c_error) {
        printf("sdcard: MMC_SELECT card failed with %d.\n", cmd.c_error);
        return -1;
    }

    card.selected = 1;
    return 0;
}

int sdcard_check_card(void)
{
    if (card.inserted == 0)
        return SDMMC_NO_CARD;

    if (card.new_card == 1)
        return SDMMC_NEW_CARD;

    return SDMMC_INSERTED;
}

int sdcard_ack_card(void)
{
    if (card.new_card == 1) {
        card.new_card = 0;
        return 0;
    }

    return -1;
}

int sdcard_start_read(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf)
{
//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) {
        printf("sdcard: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(cmdbuf, 0, sizeof(struct sdmmc_command));

    if(blk_count > 1) {
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_MULTIPLE\n"));
        cmdbuf->c_opcode = MMC_READ_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_SINGLE\n"));
        cmdbuf->c_opcode = MMC_READ_BLOCK_SINGLE;
    }
    if (card.sdhc_blockmode)
        cmdbuf->c_arg = blk_start;
    else
        cmdbuf->c_arg = blk_start * SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_data = data;
    cmdbuf->c_datalen = blk_count * SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_blklen = SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_flags = SCF_RSP_R1 | SCF_CMD_READ;
    sdhc_async_command(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("sdcard: MMC_READ_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("sdcard: async MMC_READ_BLOCK_MULTIPLE started\n"));
    else
        DPRINTF(2, ("sdcard: async MMC_READ_BLOCK_SINGLE started\n"));

    return 0;
}

int sdcard_end_read(struct sdmmc_command* cmdbuf)
{
//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) {
        printf("sdcard: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    sdhc_async_response(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("sdcard: MMC_READ_BLOCK_%s failed with %d\n", cmdbuf->c_opcode == MMC_READ_BLOCK_MULTIPLE ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(cmdbuf->c_opcode == MMC_READ_BLOCK_MULTIPLE)
        DPRINTF(2, ("sdcard: async MMC_READ_BLOCK_MULTIPLE finished\n"));
    else
        DPRINTF(2, ("sdcard: async MMC_READ_BLOCK_SINGLE finished\n"));

    return 0;
}

int sdcard_read(u32 blk_start, u32 blk_count, void *data)
{
    struct sdmmc_command cmd;

retry_single:
    // TODO: wtf is this bug
    if ((!can_sdcard_dma_addr(data) || card.multiple_fallback) && blk_count > 1) { // 
        int ret = 0;
        for (int i = 0; i < blk_count; i++)
        {
            ret = sdcard_read(blk_start + i, 1, data);
            if (ret) return ret;
            data = (void*)((intptr_t)data + SDMMC_DEFAULT_BLOCKLEN);
        }
        return ret;
    }

//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) 
    {
        printf("sdcard: READ: no card inserted.\n");
        //gpio_debug_send(0xAA);
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(&cmd, 0, sizeof(cmd));

    if(blk_count > 1) {
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_MULTIPLE\n"));
        cmd.c_opcode = MMC_READ_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_SINGLE\n"));
        cmd.c_opcode = MMC_READ_BLOCK_SINGLE;
    }
    if (card.sdhc_blockmode)
        cmd.c_arg = blk_start;
    else
        cmd.c_arg = blk_start * SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_data = data;
    cmd.c_datalen = blk_count * SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_blklen = SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_flags = SCF_RSP_R1 | SCF_CMD_READ;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error) {
        printf("sdcard: MMC_READ_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmd.c_error);
        if (blk_count > 1 && !card.multiple_fallback) {
            printf("sdcard: trying only single blocks?\n");
            card.multiple_fallback = 1;
            goto retry_single; 
        }
        else if (blk_count <= 1 && !sdcard_host.no_dma) {
            printf("sdcard: trying without DMA?\n");
            sdcard_host.no_dma = 1;
            goto retry_single;
        }
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_MULTIPLE done\n"));
    else
        DPRINTF(2, ("sdcard: MMC_READ_BLOCK_SINGLE done\n"));

    return 0;
}

#ifndef LOADER
int sdcard_start_write(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf)
{
    if (card.inserted == 0) {
        printf("sdcard: WRITE: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: WRITE: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(cmdbuf, 0, sizeof(struct sdmmc_command));

    if(blk_count > 1) {
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_MULTIPLE\n"));
        cmdbuf->c_opcode = MMC_WRITE_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_SINGLE\n"));
        cmdbuf->c_opcode = MMC_WRITE_BLOCK_SINGLE;
    }
    if (card.sdhc_blockmode)
        cmdbuf->c_arg = blk_start;
    else
        cmdbuf->c_arg = blk_start * SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_data = data;
    cmdbuf->c_datalen = blk_count * SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_blklen = SDMMC_DEFAULT_BLOCKLEN;
    cmdbuf->c_flags = SCF_RSP_R1;
    sdhc_async_command(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("sdcard: MMC_WRITE_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("sdcard: async MMC_WRITE_BLOCK_MULTIPLE started\n"));
    else
        DPRINTF(2, ("sdcard: async MMC_WRITE_BLOCK_SINGLE started\n"));

    return 0;
}

int sdcard_end_write(struct sdmmc_command* cmdbuf)
{
    if (card.inserted == 0) {
        printf("sdcard: WRITE: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: WRITE: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    sdhc_async_response(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("sdcard: MMC_WRITE_BLOCK_%s failed with %d\n", cmdbuf->c_opcode == MMC_WRITE_BLOCK_MULTIPLE ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(cmdbuf->c_opcode == MMC_WRITE_BLOCK_MULTIPLE)
        DPRINTF(2, ("sdcard: async MMC_WRITE_BLOCK_MULTIPLE finished\n"));
    else
        DPRINTF(2, ("sdcard: async MMC_WRITE_BLOCK_SINGLE finished\n"));

    return 0;
}

int sdcard_write(u32 blk_start, u32 blk_count, void *data)
{
    struct sdmmc_command cmd;

    if (sdcard_host.no_dma) {
        panic(0);
    }

retry_single:
    // TODO: wtf is this bug
    if ((!can_sdcard_dma_addr(data) || card.multiple_fallback) && blk_count > 1) { // !can_sdcard_dma_addr(data) && 
        int ret = 0;
        for (int i = 0; i < blk_count; i++)
        {
            ret = sdcard_write(blk_start + i, 1, data);
            if (ret) return ret;
            data = (void*)((intptr_t)data + SDMMC_DEFAULT_BLOCKLEN);
        }
        return ret;
    }

    if (card.inserted == 0) {
        printf("sdcard: WRITE: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (sdcard_select() < 0) {
            printf("sdcard: WRITE: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(&cmd, 0, sizeof(cmd));

    if(blk_count > 1) {
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_MULTIPLE\n"));
        cmd.c_opcode = MMC_WRITE_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_SINGLE\n"));
        cmd.c_opcode = MMC_WRITE_BLOCK_SINGLE;
    }
    if (card.sdhc_blockmode)
        cmd.c_arg = blk_start;
    else
        cmd.c_arg = blk_start * SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_data = data;
    cmd.c_datalen = blk_count * SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_blklen = SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error) {
        printf("sdcard: MMC_WRITE_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmd.c_error);
        if (blk_count > 1 && !card.multiple_fallback) {
            printf("sdcard: trying only single blocks?\n");
            card.multiple_fallback = 1;
            goto retry_single; 
        }
        else if (blk_count <= 1 && !sdcard_host.no_dma) {
            printf("sdcard: trying without DMA?\n");
            sdcard_host.no_dma = 1;
            goto retry_single;
        }
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_MULTIPLE done\n"));
    else
        DPRINTF(2, ("sdcard: MMC_WRITE_BLOCK_SINGLE done\n"));

    return 0;
}

int sdcard_wait_data(void)
{
    struct sdmmc_command cmd;

    do
    {
        DPRINTF(2, ("sdcard: MMC_SEND_STATUS\n"));
        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_SEND_STATUS;
        cmd.c_arg = ((u32)card.rca)<<16;
        cmd.c_flags = SCF_RSP_R1;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("sdcard: MMC_SEND_STATUS failed with %d\n", cmd.c_error);
            return -1;
        }
    } while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

    return 0;
}

int sdcard_get_sectors(void)
{
    if (card.inserted == 0) {
        printf("sdcard: READ: no card inserted.\n");
        return -1;
    }

    if (card.new_card == 1) {
        printf("sdcard: new card inserted but not acknowledged yet.\n");
        return -1;
    }

//  sdhc_error(sdhci->reg_base, "num sectors = %u", sdhci->num_sectors);

    return card.num_sectors;
}

void sdcard_irq(void)
{
    sdhc_intr(&sdcard_host);
}

void sdcard_init(void)
{
    struct sdhc_host_params params = {
        .attach = &sdcard_attach,
        .abort = &sdcard_abort,
        .rb = RB_SD0,
        .wb = WB_SD0,
    };

#ifdef CAN_HAZ_IRQ
    irq_enable(IRQ_SD0);
#endif
    sdhc_host_found(&sdcard_host, &params, 0, SD0_REG_BASE, 1);
}

void sdcard_exit(void)
{
#ifdef CAN_HAZ_IRQ
    irq_disable(IRQ_SD0);
#endif
    sdhc_shutdown(&sdcard_host);
}
#endif
