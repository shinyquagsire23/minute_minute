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
#include "mlc.h"
#include "gfx.h"
#include "string.h"
#include "utils.h"
#include "memory.h"

#include "latte.h"

#ifdef CAN_HAZ_IRQ
#include "irq.h"
#endif

// #define MLC_DEBUG
#define MLC_SUPPORT_WRITE

#ifdef MLC_DEBUG
static int mlcdebug = 3;
#define DPRINTF(n,s)    do { if ((n) <= mlcdebug) printf s; } while (0)
#else
#define DPRINTF(n,s)    do {} while(0)
#endif

static struct sdhc_host mlc_host;

struct mlc_ctx {
    sdmmc_chipset_handle_t handle;
    int inserted;
    int sdhc_blockmode;
    int selected;
    int new_card; // set to 1 everytime a new card is inserted

    u32 num_sectors;
    u16 rca;
};

static struct mlc_ctx card;

void mlc_attach(sdmmc_chipset_handle_t handle)
{
    memset(&card, 0, sizeof(card));

    card.handle = handle;

    DPRINTF(0, ("mlc: attached new SD/MMC card\n"));

    sdhc_host_reset(card.handle);

    if (sdhc_card_detect(card.handle)) {
        DPRINTF(1, ("card is inserted. starting init sequence.\n"));

        for (int i = 0; i < 16; i++)
        {
            mlc_needs_discover();
            if (card.inserted) break;
        }
    }
}

void mlc_abort(void) {
    struct sdmmc_command cmd;
    printf("mlc: abortion kthx\n");

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_STOP_TRANSMISSION;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
}

void mlc_needs_discover(void)
{
    struct sdmmc_command cmd;
    const u32 ocr = card.handle->ocr | SD_OCR_SDHC_CAP;

    DPRINTF(0, ("mlc: card needs discovery.\n"));
    sdhc_host_reset(card.handle);
    card.new_card = 1;

    if (!sdhc_card_detect(card.handle)) {
        DPRINTF(1, ("mlc: card (no longer?) inserted.\n"));
        card.inserted = 0;
        return;
    }

    DPRINTF(1, ("mlc: enabling power\n"));
    if (sdhc_bus_power(card.handle, ocr) != 0) {
        printf("mlc: powerup failed for card\n");
        goto out;
    }

    DPRINTF(1, ("mlc: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_400KHZ, SDMMC_TIMING_LEGACY) != 0) {
        printf("mlc: could not enable clock for card\n");
        goto out_power;
    }

    sdhc_bus_width(card.handle, 1);

    DPRINTF(1, ("mlc: sending GO_IDLE_STATE\n"));

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_GO_IDLE_STATE;
    cmd.c_flags = SCF_RSP_R0;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error) {
        printf("mlc: GO_IDLE_STATE failed with %d\n", cmd.c_error);
        goto out_clock;
    }
    DPRINTF(2, ("mlc: GO_IDLE_STATE response: %x\n", MMC_R1(cmd.c_resp)));

    int tries;
    for (tries = 100; tries > 0; tries--) {
        udelay(100000);

        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_SEND_OP_COND;
        cmd.c_arg = ocr;
        cmd.c_flags = SCF_RSP_R3;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("mlc: MMC_SEND_OP_COND failed with %d\n", cmd.c_error);
            goto out_clock;
        }

        DPRINTF(3, ("mlc: response for SEND_OP_COND: %08x\n",
                    MMC_R1(cmd.c_resp)));
        if (ISSET(MMC_R1(cmd.c_resp), MMC_OCR_MEM_READY))
            break;
    }
    if (!ISSET(cmd.c_resp[0], MMC_OCR_MEM_READY)) {
        printf("mlc: card failed to powerup.\n");
        goto out_power;
    }

    card.sdhc_blockmode = 1;

    //u8 *resp;
    u32* resp32;

    DPRINTF(2, ("mlc: MMC_ALL_SEND_CID\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_ALL_SEND_CID;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R2;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_ALL_SEND_CID failed with %d\n", cmd.c_error);
        goto out_clock;
    }

    //resp = (u8 *)cmd.c_resp;
    resp32 = (u32 *)cmd.c_resp;

    /*printf("CID: mid=%02x name='%c%c%c%c%c%c%c' prv=%d.%d psn=%02x%02x%02x%02x mdt=%d/%d\n", resp[14],
        resp[13],resp[12],resp[11],resp[10],resp[9],resp[8],resp[7], resp[6], resp[5] >> 4, resp[5] & 0xf,
        resp[4], resp[3], resp[2], resp[0] & 0xf, 2000 + (resp[0] >> 4));*/

    printf("CID: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);

    DPRINTF(2, ("mlc: SD_SEND_RELATIVE_ADDRESS\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R6;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: SD_SEND_RCA failed with %d\n", cmd.c_error);
        goto out_clock;
    }

    card.rca = MMC_R1(cmd.c_resp)>>16;
    DPRINTF(2, ("mlc: rca: %08x\n", card.rca));

    card.selected = 0;
    card.inserted = 1;

    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SEND_CSD;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R2;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SEND_CSD failed with %d\n", cmd.c_error);
        goto out_power;
    }

    //resp = (u8 *)cmd.c_resp;
    resp32 = (u32 *)cmd.c_resp;
    printf("CSD: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);

    DPRINTF(1, ("mlc: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_25MHZ, SDMMC_TIMING_LEGACY) != 0) {
        printf("mlc: could not enable clock for card\n");
        goto out_power;
    }

    mlc_select();

    DPRINTF(2, ("mlc: MMC_SEND_STATUS\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SEND_STATUS;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SEND_STATUS failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    if(MMC_R1(cmd.c_resp) & 0x3080000) {
        printf("mlc: MMC_SEND_STATUS response fail 0x%lx\n", MMC_R1(cmd.c_resp));
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(2, ("mlc: MMC_SET_BLOCKLEN\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SET_BLOCKLEN;
    cmd.c_arg = SDMMC_DEFAULT_BLOCKLEN;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SET_BLOCKLEN failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(2, ("mlc: MMC_SWITCH(0x3B70101)\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SWITCH;
    cmd.c_arg = 0x3B70101;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SWITCH(0x3B70101) failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    sdhc_bus_width(card.handle, 4);

    u8 ext_csd[512] ALIGNED(32) = {0};

    DPRINTF(2, ("mlc: MMC_SEND_EXT_CSD\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SEND_EXT_CSD;
    cmd.c_arg = 0;
    cmd.c_data = ext_csd;
    cmd.c_datalen = sizeof(ext_csd);
    cmd.c_blklen = sizeof(ext_csd);
    cmd.c_flags = SCF_RSP_R1 | SCF_CMD_ADTC | SCF_CMD_READ;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SEND_EXT_CSD failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    u8 card_type = ext_csd[0xC4];
    u32 clk = 26000;
    if(card_type & 0xE) clk = 52000;

    card.num_sectors = (u32)ext_csd[0xD4] | ext_csd[0xD5] << 8 | ext_csd[0xD6] << 16 | ext_csd[0xD7] << 24;
    printf("mlc: card_type=0x%x sec_count=0x%lx\n", card_type, card.num_sectors);

    DPRINTF(2, ("mlc: MMC_SWITCH(0x3B90101)\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SWITCH;
    cmd.c_arg = 0x3B90101;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_SWITCH(0x3B90101) failed with %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        goto out_clock;
    }

    DPRINTF(1, ("mlc: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, clk, SDMMC_TIMING_HIGHSPEED) != 0) {
        printf("mlc: could not enable clock for card\n");
        goto out_power;
    }

    return;

out_clock:
    sdhc_bus_width(card.handle, 1);
    sdhc_bus_clock(card.handle, SDMMC_SDCLK_OFF, SDMMC_TIMING_LEGACY);

out_power:
    sdhc_bus_power(card.handle, 0);
out:
    return;
}


int mlc_select(void)
{
    struct sdmmc_command cmd;

    DPRINTF(2, ("mlc: MMC_SELECT_CARD\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = MMC_SELECT_CARD;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);
    printf("%s: resp=%x\n", __FUNCTION__, MMC_R1(cmd.c_resp));
//  sdhc_dump_regs(card.handle);

//  printf("present state = %x\n", HREAD4(hp, SDHC_PRESENT_STATE));
    if (cmd.c_error) {
        printf("mlc: MMC_SELECT card failed with %d.\n", cmd.c_error);
        return -1;
    }

    card.selected = 1;
    return 0;
}

int mlc_check_card(void)
{
    if (card.inserted == 0)
        return SDMMC_NO_CARD;

    if (card.new_card == 1)
        return SDMMC_NEW_CARD;

    return SDMMC_INSERTED;
}

int mlc_ack_card(void)
{
    if (card.new_card == 1) {
        card.new_card = 0;
        return 0;
    }

    return -1;
}

int mlc_start_read(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf)
{
//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) {
        printf("mlc: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(cmdbuf, 0, sizeof(struct sdmmc_command));

    if(blk_count > 1) {
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_MULTIPLE\n"));
        cmdbuf->c_opcode = MMC_READ_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_SINGLE\n"));
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
        printf("mlc: MMC_READ_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("mlc: async MMC_READ_BLOCK_MULTIPLE started\n"));
    else
        DPRINTF(2, ("mlc: async MMC_READ_BLOCK_SINGLE started\n"));

    return 0;
}

int mlc_end_read(struct sdmmc_command* cmdbuf)
{
//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) {
        printf("mlc: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    sdhc_async_response(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("mlc: MMC_READ_BLOCK_%s failed with %d\n", cmdbuf->c_opcode == MMC_READ_BLOCK_MULTIPLE ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(cmdbuf->c_opcode == MMC_READ_BLOCK_MULTIPLE)
        DPRINTF(2, ("mlc: async MMC_READ_BLOCK_MULTIPLE finished\n"));
    else
        DPRINTF(2, ("mlc: async MMC_READ_BLOCK_SINGLE finished\n"));

    return 0;
}

int mlc_read(u32 blk_start, u32 blk_count, void *data)
{
    struct sdmmc_command cmd;

//  printf("%s(%u, %u, %p)\n", __FUNCTION__, blk_start, blk_count, data);
    if (card.inserted == 0) {
        printf("mlc: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(&cmd, 0, sizeof(cmd));

    if(blk_count > 1) {
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_MULTIPLE\n"));
        cmd.c_opcode = MMC_READ_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_SINGLE\n"));
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
        printf("mlc: MMC_READ_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmd.c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_MULTIPLE done\n"));
    else
        DPRINTF(2, ("mlc: MMC_READ_BLOCK_SINGLE done\n"));

    return 0;
}

int mlc_start_write(u32 blk_start, u32 blk_count, void *data, struct sdmmc_command* cmdbuf)
{
#ifndef MLC_SUPPORT_WRITE
    return -1;
#else
    if (card.inserted == 0) {
        printf("mlc: WRITE: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: WRITE: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(cmdbuf, 0, sizeof(struct sdmmc_command));

    if(blk_count > 1) {
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_MULTIPLE\n"));
        cmdbuf->c_opcode = MMC_WRITE_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_SINGLE\n"));
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
        printf("mlc: MMC_WRITE_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("mlc: async MMC_WRITE_BLOCK_MULTIPLE started\n"));
    else
        DPRINTF(2, ("mlc: async MMC_WRITE_BLOCK_SINGLE started\n"));

    return 0;
#endif
}

int mlc_end_write(struct sdmmc_command* cmdbuf)
{
#ifndef MLC_SUPPORT_WRITE
    return -1;
#else
    if (card.inserted == 0) {
        printf("mlc: WRITE: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: WRITE: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    sdhc_async_response(card.handle, cmdbuf);

    if (cmdbuf->c_error) {
        printf("mlc: MMC_WRITE_BLOCK_%s failed with %d\n", cmdbuf->c_opcode == MMC_WRITE_BLOCK_MULTIPLE ? "MULTIPLE" : "SINGLE", cmdbuf->c_error);
        return -1;
    }
    if(cmdbuf->c_opcode == MMC_WRITE_BLOCK_MULTIPLE)
        DPRINTF(2, ("mlc: async MMC_WRITE_BLOCK_MULTIPLE finished\n"));
    else
        DPRINTF(2, ("mlc: async MMC_WRITE_BLOCK_SINGLE finished\n"));

    return 0;
#endif
}

int mlc_write(u32 blk_start, u32 blk_count, void *data)
{
#ifndef MLC_SUPPORT_WRITE
    return -1;
#else
    struct sdmmc_command cmd;

    if (card.inserted == 0) {
        printf("mlc: READ: no card inserted.\n");
        return -1;
    }

    if (card.selected == 0) {
        if (mlc_select() < 0) {
            printf("mlc: READ: cannot select card.\n");
            return -1;
        }
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

    memset(&cmd, 0, sizeof(cmd));

    if(blk_count > 1) {
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_MULTIPLE\n"));
        cmd.c_opcode = MMC_WRITE_BLOCK_MULTIPLE;
    } else {
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_SINGLE\n"));
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
        printf("mlc: MMC_WRITE_BLOCK_%s failed with %d\n", blk_count > 1 ? "MULTIPLE" : "SINGLE", cmd.c_error);
        return -1;
    }
    if(blk_count > 1)
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_MULTIPLE done\n"));
    else
        DPRINTF(2, ("mlc: MMC_WRITE_BLOCK_SINGLE done\n"));

    return 0;
#endif
}

int mlc_wait_data(void)
{
    struct sdmmc_command cmd;

    do
    {
        DPRINTF(2, ("mlc: MMC_SEND_STATUS\n"));
        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_SEND_STATUS;
        cmd.c_arg = ((u32)card.rca)<<16;
        cmd.c_flags = SCF_RSP_R1;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("mlc: MMC_SEND_STATUS failed with %d\n", cmd.c_error);
            return -1;
        }
    } while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

    return 0;
}

int mlc_get_sectors(void)
{
    if (card.inserted == 0) {
        printf("mlc: READ: no card inserted.\n");
        return -1;
    }

    if (card.new_card == 1) {
        printf("mlc: new card inserted but not acknowledged yet.\n");
        return -1;
    }

//  sdhc_error(sdhci->reg_base, "num sectors = %u", sdhci->num_sectors);

    return card.num_sectors;
}

void mlc_irq(void)
{
    sdhc_intr(&mlc_host);
}

void mlc_init(void)
{
    struct sdhc_host_params params = {
        .attach = &mlc_attach,
        .abort = &mlc_abort,
        .rb = RB_SD2,
        .wb = WB_SD2,
    };

#ifdef CAN_HAZ_IRQ
    irql_enable(IRQL_SD2);
#endif
    sdhc_host_found(&mlc_host, &params, 0, SD2_REG_BASE, 1);
}

void mlc_exit(void)
{
#ifdef CAN_HAZ_IRQ
    irql_disable(IRQL_SD2);
#endif
    sdhc_shutdown(&mlc_host);
}
