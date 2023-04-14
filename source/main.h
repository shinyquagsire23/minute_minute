/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _MAIN_H
#define _MAIN_H

#include "types.h"
#include "menu.h"

#include "dump.h"
#include "isfs.h"

void main_quickboot_fw(void);
void main_quickboot_patch(void);
void main_boot_fw(void);
void main_boot_ppc(void);
void main_shutdown(void);
void main_reset(void);
void main_reload(void);
void main_credits(void);
void main_get_crash(void);
void main_reset_crash(void);

menu menu_main = {
    "minute", // title
    {
            "Main menu", // subtitles
    },
    1, // number of subtitles
    {
            {"Patch and boot IOS", &main_quickboot_patch}, // options
            {"Boot 'ios.img'", &main_quickboot_fw},
            {"Boot IOP firmware file", &main_boot_fw},
            {"Boot PowerPC ELF file", &main_boot_ppc},
            {"Format redNAND", &dump_format_rednand},
            {"Dump SEEPROM & OTP", &dump_seeprom_otp},
            {"Dump factory log", &dump_factory_log},
            {"Display crash log", &main_get_crash},
            {"Clear crash log", &main_reset_crash},
            {"Restart minute", &main_reload},
            {"Hardware reset", &main_reset},
            {"Power off", &main_shutdown},
            {"Credits", &main_credits},
            //{"ISFS test", &isfs_test},
    },
    13, // number of options
    0,
    0
};

#endif
