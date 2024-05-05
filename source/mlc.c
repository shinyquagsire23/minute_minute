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
static bool initialized = false;

struct mlc_ctx {
    sdmmc_chipset_handle_t handle;
    int inserted;
    int sdhc_blockmode;
    int selected;
    int new_card; // set to 1 everytime a new card is inserted

    u32 num_sectors;
    u16 rca;

    bool is_sd;
};

static struct mlc_ctx card;

void mlc_attach(sdmmc_chipset_handle_t handle)
{
    memset(&card, 0, sizeof(card));

    card.handle = handle;

    DPRINTF(0, ("mlc: attached new SD/MMC card\n"));

    if (sdhc_card_detect(card.handle)) {
        DPRINTF(1, ("card is inserted. starting init sequence.\n"));
        for (int i = 0; i < 2; i++)
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

static void _discover_emmc(void){
    struct sdmmc_command cmd;
    u32 ocr = card.handle->ocr | SD_OCR_SDHC_CAP;

    for (int tries = 0; tries < 100; tries++) {
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

    if (ISSET(MMC_R1(cmd.c_resp), SD_OCR_SDHC_CAP))
        card.sdhc_blockmode = 1;
    else
        card.sdhc_blockmode = 0;
    DPRINTF(2, ("mlc: HC: %d\n", card.sdhc_blockmode));

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

    card.rca = 0;

    //on eMMC this sets the RCA, on SD it gets the RCA
    DPRINTF(2, ("mlc: MMC_SET_RELATIVE_ADDRESS\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
    cmd.c_arg = ((u32)card.rca)<<16;
    cmd.c_flags = SCF_RSP_R6;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: SD_SEND_RCA failed with %d\n", cmd.c_error);
        goto out_clock;
    }

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
    resp32 = (u32 *)cmd.c_resp;
    printf("CSD: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);

    u8 *csd_bytes = (u8 *)cmd.c_resp;
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

    card.num_sectors = (u32)ext_csd[0xD4] | ext_csd[0xD5] << 8 | ext_csd[0xD6] << 16 | ext_csd[0xD7] << 24;
    printf("mlc: card_type=0x%x sec_count=0x%lx\n", card_type, card.num_sectors);

    if(!(card_type & 0xE)){
        printf("mlc: no SDR25 support\n");
        return;
    }

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
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_52MHZ, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }

    printf("mlc: couldn't enable highspeed clocks, trying fallback?\n");
    if (sdhc_bus_clock(card.handle, 26000, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }

    printf("mlc: couldn't enable highspeed clocks, trying another fallback?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_25MHZ, SDMMC_TIMING_LEGACY) == 0) {
        return;
    }

    printf("mlc: could not enable clock for card?\n");
    goto out_power;

out_clock:
    sdhc_bus_width(card.handle, 1);
    sdhc_bus_clock(card.handle, SDMMC_SDCLK_OFF, SDMMC_TIMING_LEGACY);

out_power:
    sdhc_bus_power(card.handle, 0);
out:
    return;
}


void mlc_needs_discover(void)
{
    struct sdmmc_command cmd;
    u32 ocr = card.handle->ocr;

    DPRINTF(0, ("mlc: card needs discovery.\n"));
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

    // Somehow this need to happen twice or the eMMC will act strange
    DPRINTF(1, ("mlc: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_400KHZ, SDMMC_TIMING_LEGACY) != 0) {
        printf("mlc: could not enable clock for card\n");
        goto out_power;
    }

    sdhc_bus_width(card.handle, 1);

    udelay(200); //need to wait at least 74 clocks -> 185usec @ 400KHz

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

    DPRINTF(1, ("mlc: sending SEND_IF_COND\n"));

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

    card.is_sd = true;

    for (int tries = 0; tries < 100; tries++) {
        udelay(100000);

        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_APP_CMD;
        cmd.c_arg = 0;
        cmd.c_flags = SCF_RSP_R1;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            if(tries == 0){
                // switch from SD mode to MMC mode
                card.is_sd = false;
                printf("mlc: is not an sd, so it is eMMC\n");
                return _discover_emmc();
            }

            printf("mlc: MMC_APP_CMD failed with %d\n", cmd.c_error);
            goto out_clock;
        }

        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = SD_APP_OP_COND;
        cmd.c_arg = ocr;
        cmd.c_flags = SCF_RSP_R3;
        sdhc_exec_command(card.handle, &cmd);

        if (cmd.c_error) {
            printf("mlc: SD_APP_OP_COND failed with %d\n", cmd.c_error);
            goto out_clock;
        }

        DPRINTF(3, ("mlc: response for SEND_IF_COND: %08x\n",
                    MMC_R1(cmd.c_resp)));
        if (ISSET(MMC_R1(cmd.c_resp), MMC_OCR_MEM_READY))
            break;

    }
    if (!ISSET(cmd.c_resp[0], MMC_OCR_MEM_READY)) {
        printf("mlc: card failed to powerup.\n");
        goto out_power;
    }

    if (ISSET(MMC_R1(cmd.c_resp), SD_OCR_SDHC_CAP))
        card.sdhc_blockmode = 1;
    else
        card.sdhc_blockmode = 0;
    DPRINTF(2, ("mlc: HC: %d\n", card.sdhc_blockmode));

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

    u8 *resp = (u8 *)cmd.c_resp;

    printf("CID: mid=%02x name='%c%c%c%c%c%c%c' prv=%d.%d psn=%02x%02x%02x%02x mdt=%d/%d\n", resp[14],
        resp[13],resp[12],resp[11],resp[10],resp[9],resp[8],resp[7], resp[6], resp[5] >> 4, resp[5] & 0xf,
        resp[4], resp[3], resp[2], resp[0] & 0xf, 2000 + (resp[0] >> 4));


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


    u32 *resp32 = (u32 *)cmd.c_resp;
    printf("CSD: %08lX%08lX%08lX%08lX\n", resp32[0], resp32[1], resp32[2], resp32[3]);
    
    u16 ccc = SD_CSD_CCC(resp32);
    printf("CCC (hex): %03X\n", ccc);

    u8 *csd_bytes = (u8 *)cmd.c_resp;
    if (csd_bytes[13] == 0xe) { // sdhc
        unsigned int c_size = csd_bytes[7] << 16 | csd_bytes[6] << 8 | csd_bytes[5];
        printf("mlc: sdhc mode, c_size=%u, card size = %uk\n", c_size, (c_size + 1)* 512);
        card.num_sectors = (c_size + 1) * 1024; // number of 512-byte sectors
    }

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

    if(!(ccc & SD_CSD_CCC_CMD6)){
        printf("mlc: CMD6 not supported, stay in SDR12\n");
        return;
    }
    u8 mode_status[64] ALIGNED(32) = {0};
    DPRINTF(2, ("mlc: SWITCH FUNC Mode 0\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SWITCH_FUNC;
    cmd.c_arg = 0x00FFFFF1;
    cmd.c_data = mode_status;
    cmd.c_datalen = sizeof(mode_status);
    cmd.c_blklen = sizeof(mode_status);
    cmd.c_flags = SCF_RSP_R1 | SCF_CMD_ADTC | SCF_CMD_READ;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: SWITCH FUNC Mode 0 %d\n", cmd.c_error);
        card.inserted = card.selected = 0;
        //goto out_clock;
        return; // 1.0 card, which doesn't support CMD6
    }

    if(mode_status[16] != 1){
        // Does not SD25 (52MHz), so leave 25MHz
        printf("mlc: doesn't support SDR25, staying at SDR12\n");
        return;
    }
    DPRINTF(2, ("mlc: SWITCH FUNC Mode 1\n"));
    memset(&cmd, 0, sizeof(cmd));
    cmd.c_opcode = SD_SWITCH_FUNC;
    cmd.c_arg = 0x80FFFFF1;
    cmd.c_data = mode_status;
    cmd.c_datalen = sizeof(mode_status);
    cmd.c_blklen = sizeof(mode_status);
    cmd.c_flags = SCF_RSP_R1 | SCF_CMD_ADTC | SCF_CMD_READ;
    sdhc_exec_command(card.handle, &cmd);
    if(mode_status[16] != 1){
        printf("mlc: switch to SDR25 failed, staying at SDR12\n");
        return;
    }

    DPRINTF(1, ("mlc: enabling clock\n"));
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_52MHZ, SDMMC_TIMING_HIGHSPEED) == 0) {
        return;
    }

    printf("mlc: couldn't enable highspeed clocks, trying another fallback?\n");
    if (sdhc_bus_clock(card.handle, SDMMC_SDCLK_25MHZ, SDMMC_TIMING_LEGACY) == 0) {
        return;
    }

    printf("mlc: could not enable clock for card?\n");
    goto out_power;

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
    } else if(MMC_R1(cmdbuf->c_resp) & MMC_R1_ANY_ERROR){
        printf("mlc: reported read error. status: %08lx\n", MMC_R1(cmdbuf->c_resp));
        return -2;
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
    } else if(MMC_R1(cmdbuf->c_resp) & MMC_R1_ANY_ERROR){
        printf("mlc: reported write error. status: %08lx\n", MMC_R1(cmdbuf->c_resp));
        return -2;
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

u32 mlc_get_sectors(void)
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


static int mlc_do_erase(u32 start, u32 end){
#ifndef MLC_SUPPORT_WRITE
    return -1;
#else
    struct sdmmc_command cmd = { 0 };

    cmd.c_opcode = card.is_sd ? SD_ERASE_WR_BLK_START:MMC_ERASE_GROUP_START;
    cmd.c_arg = start;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_ERASE_GROUP_START failed with %d\n", cmd.c_error);
        return -1;
    }

    cmd.c_opcode = card.is_sd ? SD_ERASE_WR_BLK_END : MMC_ERASE_GROUP_END;
    cmd.c_arg = end;
    cmd.c_flags = SCF_RSP_R1;
    sdhc_exec_command(card.handle, &cmd);
    if (cmd.c_error) {
        printf("mlc: MMC_ERASE_GROUP_END failed with %d\n", cmd.c_error);
        return -1;
    }

    cmd.c_opcode = MMC_ERASE;
    cmd.c_arg = 0;
    cmd.c_flags = SCF_RSP_R1B;
    sdhc_exec_command(card.handle, &cmd);

    if (cmd.c_error) {
        printf("mlc: MMC_ERASE failed with %d\n", cmd.c_error);
        return -1;
    }
    if(MMC_R1(cmd.c_resp)&~0x900)
        printf("ERASE: resp=%x\n", MMC_R1(cmd.c_resp));

    do {
        //udelay(1);
        memset(&cmd, 0, sizeof(cmd));
        cmd.c_opcode = MMC_SEND_STATUS;
        cmd.c_arg = ((u32)card.rca)<<16;
        cmd.c_flags = SCF_RSP_R1;
        sdhc_exec_command(card.handle, &cmd);
    } while (cmd.c_error == 116 || MMC_R1(cmd.c_resp) == 0x800); //WHY?

    if(cmd.c_error){
        printf("mlc: MMC_SEND_STATUS failed with 0x%d\n", cmd.c_error);
        return -1;
    }

    if(MMC_R1(cmd.c_resp) != 0x900){
        printf("mlc: MMC_SEND_STATUS response 0x%08lx\n", MMC_R1(cmd.c_resp));
        return -2;
    }

    return 0;
#endif
}


int mlc_erase(void){
#ifndef MLC_SUPPORT_WRITE
    return -1;
#else
    u32 size = mlc_get_sectors();
    if(!size){
        printf("ERROR mlc has 0 bytes");
        return -3;
    }

    if(size==-1){
        printf("ERROR mlc not detected");
        return -4;
    }

    const u32 erase_block_size = 0x20000;

    for(u32 base = 0; base<size; base+=erase_block_size){
        //if(!(base%40000))
            printf("Erase 0x%08lx/%08lx\n", base, size);
        mlc_do_erase(base, min(size, base+erase_block_size)-1);

    }
 
    return 0;
#endif
}

static void _mlc_do_init(void){
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

int mlc_init(void)
{
    if(!initialized){
        printf("Initializing MLC...\n");
        _mlc_do_init();
        int res = mlc_ack_card();
        if(res)
            return res;
    }

    initialized = true;

    if(mlc_check_card() == SDMMC_NO_CARD) {
        printf("Error while initializing MLC.\n");
        return -1;
    }
    return 0;
}

void mlc_exit(void)
{
    if(!initialized)
        return;
#ifdef CAN_HAZ_IRQ
    irql_disable(IRQL_SD2);
#endif
    sdhc_shutdown(&mlc_host);
    initialized = false;
}
