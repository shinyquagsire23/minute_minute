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

#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "gpio.h"
#include "smc.h"
#include "console.h"
#include "gfx.h"
#include "main.h"

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

void intcon_show_help(void)
{
    printf("Valid commands: exit, quit, reset, restart, shutdown, smc, help, ?\n");
}

void intcon_smc_cmd(int argc, char** argv)
{
    if (argc < 2) {
smc_usage:
        printf("Usage: smc raw <val>\n");
        printf("       smc read <addr>\n");
        printf("       smc write <addr> <val>\n");
        return;
    }

    if (!strcmp(argv[1], "raw")) {
        if (argc < 3) {
            goto smc_usage;
        }
        uint8_t val = strtol(argv[2], NULL, 0);

        smc_write_raw(val);
        printf("raw %02x\n", val);
    }
    else if (!strcmp(argv[1], "read")) {
        if (argc < 3) {
            goto smc_usage;
        }
        uint8_t addr = strtol(argv[2], NULL, 0);
        uint8_t val = 0;
        uint8_t len = 1;
        if (argc > 3) {
            len = strtol(argv[3], NULL, 0);
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
    else if (!strcmp(argv[1], "write")) {
        if (argc < 4) {
            goto smc_usage;
        }
        uint8_t addr = strtol(argv[2], NULL, 0);
        uint8_t val = strtol(argv[3], NULL, 0);
        printf("write %02x: %02x\n", addr, val);

        smc_write_register(addr, val);
    }
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
    else if (!strcmp(cmd, "smc")) {
        intcon_smc_cmd(argc, argv);
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
    printf("\033[0G\033[0K");
    printf("\033[0G> %s\n", intcon_current_command);

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
    printf("\033[0G\033[0K");
    printf("\033[0G> %s", intcon_current_command);
    printf("\033[0G");
    for (int i = 0; i < 2+intcon_current_command_cursor_idx; i++)
    {
        printf("\033[C");
    }

    intcon_dirty = 0;
}

void intcon_show(void)
{
    char serial_tmp[256];
    int serial_len = 0;

    int parsing_csi = 0;
    int parsing_escape_code = 0;

    intcon_dirty = 1;
    intcon_active = 1;
    while(intcon_active)
    {
        intcon_draw();

        serial_poll();
        serial_len = serial_in_read(serial_tmp);
        for (int i = 0; i < serial_len; i++) {
            if (parsing_csi) {
                if (parsing_csi == 1 && serial_tmp[i] >= '0' && serial_tmp[i] <= '1') {
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
            }
            else if (parsing_escape_code)
            {
                if (parsing_escape_code == 1 && serial_tmp[i] == '[') {
                    parsing_csi = 1;
                    parsing_escape_code = 0;
                    continue;
                }
                
                parsing_escape_code++;
            }
            else {
                switch (serial_tmp[i])
                {
                case '\033':
                    parsing_escape_code = 1;
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