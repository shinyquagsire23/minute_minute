/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "interactive_console.h"

#ifndef MINUTE_BOOT1

#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include "serial.h"
#include "smc.h"
#include "console.h"
#include "gfx.h"
#include "main.h"
#include "ancast.h"
#include "utils.h"
#include "sha.h"
#include "asic.h"

#define INTCON_HISTORY_DEPTH (64)
#define INTCON_COMMAND_MAX_LEN (256)

static char* intcon_command_history[INTCON_HISTORY_DEPTH];

int intcon_cmd_history_idx = -1;
int intcon_dirty = 0;
int intcon_active = 0;
int intcon_current_command_cursor_idx = 0;
char intcon_current_command[INTCON_COMMAND_MAX_LEN];

void intcon_init(void)
{
    gfx_clear(GFX_ALL, BLACK);
    console_init();

    for (int i = 0; i < INTCON_HISTORY_DEPTH; i++)
    {
        intcon_command_history[i] = NULL;
    }
    memset(intcon_current_command, 0, sizeof(intcon_current_command));
    intcon_current_command_cursor_idx = 0;
    intcon_cmd_history_idx = -1;
    intcon_active = 0;
    intcon_dirty = 1;
}

void intcon_insert_character(char c)
{
    char tmp[INTCON_COMMAND_MAX_LEN+1];
    if (intcon_current_command_cursor_idx >= INTCON_COMMAND_MAX_LEN) {
        intcon_current_command_cursor_idx = INTCON_COMMAND_MAX_LEN-1;
        return;
    }

    memcpy(tmp, intcon_current_command, intcon_current_command_cursor_idx);
    tmp[intcon_current_command_cursor_idx] = c;
    memcpy(tmp + intcon_current_command_cursor_idx + 1, intcon_current_command + intcon_current_command_cursor_idx, 255-intcon_current_command_cursor_idx);

    memcpy(intcon_current_command, tmp, INTCON_COMMAND_MAX_LEN);

    intcon_current_command_cursor_idx += 1;
    intcon_dirty = 1;
}

void intcon_backspace()
{
    char tmp[INTCON_COMMAND_MAX_LEN+1];
    if (intcon_current_command_cursor_idx <= 0) {
        intcon_current_command_cursor_idx = 0;
        return;
    }
    memcpy(tmp, intcon_current_command, intcon_current_command_cursor_idx-1);
    memcpy(tmp + intcon_current_command_cursor_idx - 1, intcon_current_command + intcon_current_command_cursor_idx, 254-intcon_current_command_cursor_idx);

    memcpy(intcon_current_command, tmp, INTCON_COMMAND_MAX_LEN);
    intcon_current_command_cursor_idx -= 1;
    intcon_dirty = 1;
}

void intcon_delete()
{
    char tmp[INTCON_COMMAND_MAX_LEN+1];
    if (intcon_current_command_cursor_idx >= INTCON_COMMAND_MAX_LEN) {
        intcon_current_command_cursor_idx = INTCON_COMMAND_MAX_LEN-1;
        return;
    }
    memcpy(tmp, intcon_current_command, intcon_current_command_cursor_idx);
    memcpy(tmp + intcon_current_command_cursor_idx, intcon_current_command + intcon_current_command_cursor_idx + 1, 254-intcon_current_command_cursor_idx);

    memcpy(intcon_current_command, tmp, INTCON_COMMAND_MAX_LEN);
    intcon_dirty = 1;
}

void intcon_show_help(void)
{
    printf("Valid commands: exit, quit, reset, restart, shutdown, smc, peek, poke, set, clear, help, ?\n");
}

void intcon_smc_cmd(int argc, char** argv)
{
    if (argc < 2) {
smc_usage:
        printf("Usage: smc raw <val>\n");
        printf("       smc read <addr> [len]\n");
        printf("       smc readmulti <addr> <times>\n");
        printf("       smc write <addr> <val>\n");
        printf("       smc writemulti <addr> <len>\n");
        printf("       smc writeseq <addr> <val 1> [val 2] ...\n");
        return;
    }

    if (!strcmp(argv[1], "raw")) {
        if (argc < 3) {
            goto smc_usage;
        }
        u8 val = strtoll(argv[2], NULL, 0);

        smc_write_raw(val);
        printf("raw %02x\n", val);
    }
    else if (!strcmp(argv[1], "read")) {
        if (argc < 3) {
            goto smc_usage;
        }
        u8 addr = strtoll(argv[2], NULL, 0);
        u8 val = 0;
        u8 len = 1;
        if (argc > 3) {
            len = strtoll(argv[3], NULL, 0);
        }

        printf("read %02x:", addr);

        for (int i = 0; i < len; i++) {
            if (i % 16 == 0) {
                printf("\n");
            }
            smc_read_register(addr + i, &val);
            printf("%02x ", val);
        }
        printf("\n");
    }
    else if (!strcmp(argv[1], "readmulti")) {
        if (argc < 4) {
            goto smc_usage;
        }
        u8 addr = strtoll(argv[2], NULL, 0);
        u8 val = 0;
        u8 len = strtoll(argv[3], NULL, 0);

        printf("read %02x (len %02x):", addr, len);

        u8* tmp = malloc(len);
        memset(tmp, 0xFF, len);
        smc_read_register_multiple(addr-1, tmp, len); // why the -1?

        for (int i = 0; i < len; i++) {
            if (i % 16 == 0) {
                printf("\n");
            }
            printf("%02x ", tmp[i]);
        }
        printf("\n");
        free(tmp);
    }
    else if (!strcmp(argv[1], "write")) {
        if (argc < 4) {
            goto smc_usage;
        }
        u8 addr = strtoll(argv[2], NULL, 0);
        u8 val = strtoll(argv[3], NULL, 0);
        printf("write %02x: %02x\n", addr, val);

        smc_write_register(addr, val);
    }
    else if (!strcmp(argv[1], "writeseq")) {
        if (argc < 4) {
            goto smc_usage;
        }
        int len = argc-3;
        u8 addr = strtoll(argv[2], NULL, 0);
        
        for (int i = 0; i < len; i++) {
            u8 val = strtoll(argv[3+i], NULL, 0);
            
            smc_write_register(addr + i, val);
        }
    }
    else if (!strcmp(argv[1], "writemulti")) {
        if (argc < 4) {
            goto smc_usage;
        }
        u8* tmp = malloc(argc-3);
        for (int i = 3; i < argc; i++)
        {
            tmp[i-3] = strtoll(argv[i], NULL, 0);
        }
        u8 addr = strtoll(argv[2], NULL, 0);

        smc_write_register_multiple(addr, tmp, argc-3);
    }
    else if (!strcmp(argv[1], "test")) {
        if (argc < 3) {
            goto smc_usage;
        }
        u16 addr = strtoll(argv[2], NULL, 0);
        u8 val = 0;
        u8 last_val = 0xFF;
        printf("test %04x\n", addr);

        smc_write_register(0x73, (addr >> 8) & 0xFF);
        smc_write_register(0x74, addr & 0xFF);

        smc_read_register(0x73, &val);
        printf("0x73: %02x\n", val);
        last_val = val;
        smc_read_register(0x74, &val);
        printf("0x74: %02x\n", val);
        for (int i = 0; i < 5; i++)
        {
            smc_read_register(0x73, &val);
            if (val != last_val) {
                printf("0x73: %02x\n", val);
            }
            last_val = val;
        }
        smc_read_register(0x74, &val);
        printf("0x74: %02x\n", val);

        smc_read_register(0x76, &val);
        printf("0x76: %02x\n", val);
        last_val = val;

        for (int i = 0; i < 5; i++)
        {
            smc_read_register(0x76, &val);
            if (val != last_val) {
                printf("0x76: %02x\n", val);
            }
            last_val = val;
        }
    }
    else if (!strcmp(argv[1], "test2")) {
        if (argc < 4) {
            goto smc_usage;
        }
        u16 addr_start = strtoll(argv[2], NULL, 0);
        u16 addr_len = strtoll(argv[3], NULL, 0);
        printf("test %04x %04x\n", addr_start, addr_len);

        int idx = 0;
        for (u32 addr = addr_start; addr < addr_start+addr_len; addr++)
        {
            u8 val = 0;
            u8 last_val = 0xFF;
            
            if (idx && idx % 16 == 0) {
                printf("\n");
            }
            idx++;

            smc_write_register(0x73, (addr >> 8) & 0xFF);
            smc_write_register(0x74, addr & 0xFF);

            int bad_addr = 0;
            for (int i = 0; i < 4; i++)
            {
                //if (!bad_addr)
                {
                    val = 0;
                    smc_read_register(0x73, &val);
                    if (val != (addr >> 8) & 0xFF) {
                        bad_addr = 1;
                    }
                    else {
                        bad_addr = 0;
                    }
                }

                val = 0;
                smc_read_register(0x70, &val);
                if (val) {
                    break;
                }
            }

            if (val == 0xFF)
            {
                printf("?? ");
            }
            else if (!val) {
                if (bad_addr) {
                    printf("nn ");
                }
                else {
                    printf("xx ");
                }                
            }
            else {
                //printf("%02x? ", val);
                smc_read_register(0x76, &val);
                printf("%02x ", val);
            }
            
        }
        printf("\n");
    }
    else if (!strcmp(argv[1], "test3")) {
        if (argc < 4) {
            goto smc_usage;
        }
        u16 addr_start = strtoll(argv[2], NULL, 0);
        u16 addr_len = strtoll(argv[3], NULL, 0);
        printf("test3 %04x %04x\n", addr_start, addr_len);

        int idx = 0;
        for (u32 addr = addr_start; addr < addr_start+addr_len; addr++)
        {
            u8 val = 0;
            u8 last_val = 0xFF;
            
            if (idx && idx % 16 == 0) {
                printf("\n");
            }
            idx++;

            smc_write_register(0x72, addr & 0xFF);

            smc_write_register(0x73, (addr >> 8) & 0xFF);
            smc_write_register(0x74, 4);

            int bad_addr = 0;
            for (int i = 0; i < 4; i++)
            {
                //if (!bad_addr)
                {
                    val = 0;
                    smc_read_register(0x73, &val);
                    if (val != (addr >> 8) & 0xFF) {
                        bad_addr = 1;
                    }
                    else {
                        bad_addr = 0;
                    }
                }

                val = 0;
                smc_read_register(0x70, &val);
                if (val) {
                    break;
                }
            }

            if (val == 0xFF)
            {
                printf("?? ");
            }
            else if (!val) {
                if (bad_addr) {
                    printf("nn ");
                }
                else {
                    printf("xx ");
                }                
            }
            else {
                //printf("%02x? ", val);
                smc_read_register(0x76, &val);
                printf("%02x ", val);
            }
            
        }
        printf("\n");
    }
}

void intcon_memory_cmd(int argc, char** argv)
{
    if (!strcmp(argv[0], "peek"))
    {
        if (argc < 2) {
            printf("Usage: peek <addr>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = read32(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "poke"))
    {
        if (argc < 3) {
            printf("Usage: poke <addr> <val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        write32(addr, val);
        val = read32(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "set"))
    {
        if (argc < 3) {
            printf("Usage: set <addr> <OR val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        set32(addr, val);
        val = read32(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "clear"))
    {
        if (argc < 3) {
            printf("Usage: clear <addr> <AND ~val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        clear32(addr, val);
        val = read32(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "peek16"))
    {
        if (argc < 2) {
            printf("Usage: peek16 <addr>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = read16(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "poke16"))
    {
        if (argc < 3) {
            printf("Usage: poke16 <addr> <val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        write16(addr, val);
        val = read16(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "set16"))
    {
        if (argc < 3) {
            printf("Usage: set16 <addr> <OR val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        set16(addr, val);
        val = read16(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "clear16"))
    {
        if (argc < 3) {
            printf("Usage: clear16 <addr> <AND ~val>\n");
            return;
        }
        u32 addr = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        clear16(addr, val);
        val = read16(addr);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "abifr"))
    {
        if (argc < 2) {
            printf("Usage: abifr <offs>\n");
            return;
        }
        u32 offs = strtoll(argv[1], NULL, 0);
        u32 val = abif_basic_read32(offs);
        printf("%08x\n", val);
    }
    else if (!strcmp(argv[0], "abifw"))
    {
        if (argc < 3) {
            printf("Usage: abifw <offs> <val>\n");
            return;
        }
        u32 offs = strtoll(argv[1], NULL, 0);
        u32 val = strtoll(argv[2], NULL, 0);
        abif_basic_write32(offs, val);
        val = abif_basic_read32(offs);
        printf("%08x\n", val);
    }
}

int intcon_upload(const char* fpath)
{
    u8 serial_tmp[256];
    u8 last_12[12];
    int serial_len = 0;
    int serial_idx = 0;
    int serial_remainder;
    u32 transfer_len;
    u32 bytes_left;
    int is_synced = 0;
    u8* out_iter;
    FILE* f_fw;
    int attempts;

    const u8 magic_upld[13] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0x50, 0x4C, 0x44, 0x0a};
    const u8 magic_sync[8] = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA};

    serial_allow_zeros();
    for (int i = 0; i < sizeof(magic_upld); i++) {
        serial_send(magic_upld[i]);
    }
    serial_printf("%s\n", fpath);

    out_iter = (u8*)ALL_PURPOSE_TMP_BUF;

    memset(last_12, 0, sizeof(last_12));
    attempts = 5000;
    while(--attempts)
    {
        udelay(1000);

        serial_poll();
        serial_len = serial_in_read(serial_tmp);
        serial_idx = 0;
        if (serial_len) {
            attempts = 5000;
        }

        for (int i = 0; i < serial_len; i++) {
            serial_idx++;
            memmove(last_12, last_12+1, 11);
            last_12[11] = serial_tmp[i];

            if (!memcmp(last_12, magic_sync, 8)) {
                is_synced = 1;
                break;
            }
        }
        if (is_synced) break;
    }

    if (attempts <= 0) {
        goto fail;
    }

    f_fw = fopen(fpath, "wb");
    if(!f_fw)
    {
        printf("Failed to open `%s` for writing.\n", fpath);
        goto fail;
    }

    serial_remainder = serial_len-serial_idx;
    transfer_len = read32_unaligned(last_12+8);
    bytes_left = transfer_len;
    //printf("Transferring 0x%08x bytes (%x)\n", transfer_len, serial_remainder);

    if (bytes_left < serial_remainder) {
        memcpy(out_iter, serial_tmp+serial_idx, bytes_left);
        bytes_left = 0;
        goto done;
    }

    if (serial_remainder)
    {
        memcpy(out_iter, serial_tmp+serial_idx, serial_remainder);
        out_iter += serial_remainder;
        bytes_left -= serial_remainder;
    }

    attempts = 5000;
    while (--attempts)
    {
        //for (int i = 0; i < 128; i++)
        {
            serial_poll();
        }
        serial_len = serial_in_read(serial_tmp);
        if (serial_len) {
            attempts = 5000;
        }

        if (serial_len > bytes_left) {
            serial_len = bytes_left;
        }

        //printf("%x %x\n", bytes_left, serial_len);

        memcpy(out_iter, serial_tmp, serial_len);
        out_iter += serial_len;
        bytes_left -= serial_len;
        if (bytes_left <= 0) {
            break;
        }
    }

    if (attempts <= 0) {
        goto fail;
    }

    fwrite((u8*)ALL_PURPOSE_TMP_BUF, transfer_len, 1, f_fw);

done:
    serial_disallow_zeros();
    printf("Transfer complete!\n");
    u32 hash[SHA_HASH_WORDS] = {0};
    sha_hash((void*)ALL_PURPOSE_TMP_BUF, hash, transfer_len);

    printf("sha1:   %08lX%08lX%08lX%08lX%08lX\n", hash[0], hash[1], hash[2], hash[3], hash[4]);
    fclose(f_fw);

    return 0;
fail:
    serial_disallow_zeros();
    printf("Transfer failed.\n");
    fclose(f_fw);

    console_power_to_exit();
    return 1;
}

void intcon_handle_cmd(const char* pCmd)
{
    char cmd[INTCON_COMMAND_MAX_LEN];
    strcpy(cmd, pCmd);

    char* last_start = cmd;
    int argc = 0;
    char* argv[16] = {0};
    for (int i = 0; i < INTCON_COMMAND_MAX_LEN-1; i++) {
        if (argc >= 16) break;
        if (!cmd[i]) {
            argv[argc] = last_start;
            last_start = &cmd[i+1];
            if (strlen(argv[argc]) > 0) {
                argc++;
            }
            break;
        }

        if (cmd[i] == ' ') {
            argv[argc] = last_start;
            cmd[i] = 0;
            last_start = &cmd[i+1];
            if (strlen(argv[argc]) > 0) {
                argc++;
            }
        }
    }

    if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) {
        intcon_active = 0;
    }
    else if (!strcmp(cmd, "reset") || !strcmp(cmd, "restart")) {
        main_reset();
        intcon_active = 0;
    }
    else if (!strcmp(cmd, "shutdown")) {
        main_shutdown();
        intcon_active = 0;
    }
    else if (!strcmp(cmd, "up") || !strcmp(cmd, "upload")) {
        if (!intcon_upload("sdmc:/fw.img")) {
            main_reload();
            intcon_active = 0;
        }
        
    }
    else if (!strcmp(cmd, "upp") || !strcmp(cmd, "uploadpatch")) {
        if (!intcon_upload("sdmc:/ios.patch")) {
            main_quickboot_patch();
            intcon_active = 0;
        }
    }
    else if (!strcmp(cmd, "uppl") || !strcmp(cmd, "uploadplugin")) {
        if (!intcon_upload("sdmc:/wiiu/ios_plugins/wafel_core.ipx")) {
            main_quickboot_patch();
            intcon_active = 0;
        }
    }
    else if (!strcmp(cmd, "smc")) {
        intcon_smc_cmd(argc, argv);
    }
    else if (!strcmp(cmd, "peek") || !strcmp(cmd, "poke") || !strcmp(cmd, "set") || !strcmp(cmd, "clear") 
             || !strcmp(cmd, "peek16") || !strcmp(cmd, "poke16") || !strcmp(cmd, "set16") || !strcmp(cmd, "clear16")
             || !strcmp(cmd, "abifr") || !strcmp(cmd, "abifw")) {
        intcon_memory_cmd(argc, argv);
    }
    else if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
        intcon_show_help();
    }
    else {
        printf("minute: command not found: %s\n", intcon_current_command);
    }
}

void intcon_submit_cmd(void)
{
    serial_printf("\033[0G\033[0K");
    serial_printf("\033[0G");
    printf("> %s\n", intcon_current_command);

    intcon_handle_cmd(intcon_current_command);
    

    if (intcon_command_history[INTCON_HISTORY_DEPTH-1]) {
        free(intcon_command_history[INTCON_HISTORY_DEPTH-1]);
    }

    for (int i = INTCON_HISTORY_DEPTH-1; i >= 1; i--)
    {
        intcon_command_history[i] = intcon_command_history[i-1];
    }
    intcon_command_history[0] = malloc(strlen(intcon_current_command)+1);
    strcpy(intcon_command_history[0], intcon_current_command);

    memset(intcon_current_command, 0, INTCON_COMMAND_MAX_LEN);
    intcon_current_command_cursor_idx = 0;
    intcon_cmd_history_idx = -1;

    intcon_dirty = 1;
}

void intcon_draw(void)
{
    if (!intcon_dirty) {
        return;
    }
    //console_flush();
    //console_add_text(intcon_current_command);
    serial_printf("\033[0G\033[0K");
    serial_printf("\033[0G> %s", intcon_current_command);
    serial_printf("\033[0G");
    for (int i = 0; i < 2+intcon_current_command_cursor_idx; i++)
    {
        serial_printf("\033[C");
    }

    intcon_dirty = 0;
}

void intcon_show(void)
{
    char serial_number_tmp[256];
    char serial_tmp[256];
    int serial_len = 0;

    int serial_number_tmp_len = 0;
    int parsing_csi_num = 0;
    int parsing_csi = 0;
    int parsing_escape_code = 0;

    intcon_dirty = 1;
    intcon_active = 1;
    while(intcon_active)
    {
        intcon_draw();

        serial_disallow_zeros();
        serial_poll();
        serial_len = serial_in_read(serial_tmp);
        for (int i = 0; i < serial_len; i++) {
            if (serial_tmp[i] == 0) continue;
            if (parsing_csi) {
                //printf("%c", serial_tmp);
                
                if (serial_tmp[i] >= '0' && serial_tmp[i] <= '9') {
                    serial_number_tmp[serial_number_tmp_len++] = serial_tmp[i];
                    parsing_csi_num = atoi(serial_number_tmp);
                    parsing_csi++;
                    continue;
                }
                else if (serial_tmp[i] == 'A') {
                    //menu_prev_selection();

                    intcon_cmd_history_idx++;
                    if (intcon_cmd_history_idx >= INTCON_HISTORY_DEPTH) {
                        intcon_cmd_history_idx = INTCON_HISTORY_DEPTH-1;
                    }

                    if (intcon_cmd_history_idx >= 0 && intcon_command_history[intcon_cmd_history_idx]) {
                        strcpy(intcon_current_command, intcon_command_history[intcon_cmd_history_idx]);
                        intcon_current_command_cursor_idx = strlen(intcon_current_command);
                    }
                    else {
                        intcon_cmd_history_idx--;
                        if (intcon_cmd_history_idx <= -1) {
                            intcon_cmd_history_idx = -1;
                            memset(intcon_current_command, 0, INTCON_COMMAND_MAX_LEN);
                            intcon_current_command_cursor_idx = 0;
                        }
                    }
                    intcon_dirty = 1;
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'B') {
                    //menu_next_selection();

                    intcon_cmd_history_idx--;

                    if (intcon_cmd_history_idx >= 0 && intcon_command_history[intcon_cmd_history_idx]) {
                        strcpy(intcon_current_command, intcon_command_history[intcon_cmd_history_idx]);
                        intcon_current_command_cursor_idx = strlen(intcon_current_command);
                    }
                    else {
                        if (intcon_cmd_history_idx <= -1) {
                            intcon_cmd_history_idx = -1;
                            memset(intcon_current_command, 0, INTCON_COMMAND_MAX_LEN);
                            intcon_current_command_cursor_idx = 0;
                        }
                    }

                    intcon_dirty = 1;
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'C') {
                    intcon_current_command_cursor_idx++;
                    if (intcon_current_command_cursor_idx <= 0) {
                        intcon_current_command_cursor_idx = 0;
                    }
                    if (intcon_current_command_cursor_idx >= strlen(intcon_current_command)) {
                        intcon_current_command_cursor_idx = strlen(intcon_current_command);
                    }
                    intcon_dirty = 1;
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == 'D') {
                    intcon_current_command_cursor_idx--;
                    if (intcon_current_command_cursor_idx <= 0) {
                        intcon_current_command_cursor_idx = 0;
                    }
                    if (intcon_current_command_cursor_idx >= strlen(intcon_current_command)) {
                        intcon_current_command_cursor_idx = strlen(intcon_current_command);
                    }
                    intcon_dirty = 1;
                    parsing_csi = 0;
                    continue;
                }
                else if (serial_tmp[i] == '~') {
                    //serial_printf("%u\n", parsing_csi_num);
                    if (parsing_csi_num == 3) {
                        intcon_delete();
                    }
                    parsing_csi = 0;
                    continue;
                }
                else {
                    serial_printf("\n\nunk CSI? [%u%c\n", parsing_csi_num, serial_tmp[i]);
                    parsing_csi = 0;
                    continue;
                }
            }
            else if (parsing_escape_code)
            {
                if (parsing_escape_code == 1 && serial_tmp[i] == '[') {
                    parsing_csi = 1;
                    parsing_escape_code = 0;
                    continue;
                }
                
                //parsing_escape_code++;
                parsing_escape_code = 0;
            }
            else {
                switch (serial_tmp[i])
                {
                case '\033':
                    parsing_escape_code = 1;
                    memset(serial_number_tmp, 0, sizeof(serial_number_tmp));
                    serial_number_tmp_len = 0;
                    break;
                case '\b':
                    intcon_backspace();
                    break;
                case '\n':
                case '\r':
                    intcon_submit_cmd();
                    break;
                default:
                    intcon_insert_character(serial_tmp[i]);
                }
            }
        }

        //u8 input = smc_get_events();

        //TODO: Double press to go back? Or just add "Back" options

        //if(input & SMC_EJECT_BUTTON) menu_select();
        //if(input & SMC_POWER_BUTTON) menu_next_selection();
    }
}

#endif // MINUTE_BOOT1