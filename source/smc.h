/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _SMC_H
#define _SMC_H

#include "types.h"

#define SMC_POWER_BUTTON (0x40)
#define SMC_EJECT_BUTTON (0x20)
#define SMC_DISK_INSERT  (0x10)
#define SMC_TIMER        (0x08)
#define SMC_BT_IRQ       (0x04)
#define SMC_WAKE0        (0x02)
#define SMC_WAKE1        (0x01)

#define LED_OFF   (0x0)
#define LED_ON    (0x1)
#define LED_PULSE (0x2)

#define LEDRAW_BLUE_EN       (0x20)
#define LEDRAW_BLUE_PULSE_EN    (0x10)
#define LEDRAW_RED_EN        (0x08)
#define LEDRAW_RED_PULSE_EN     (0x04)
#define LEDRAW_YELLOW_EN     (0x02)
#define LEDRAW_YELLOW_PULSE_EN  (0x01)

#define LEDRAW_RED           (LEDRAW_RED_EN)
#define LEDRAW_BLUE          (LEDRAW_BLUE_EN)
#define LEDRAW_YELLOW        (LEDRAW_YELLOW_EN)
#define LEDRAW_PURPLE        (LEDRAW_BLUE_EN | LEDRAW_RED_EN)
#define LEDRAW_ORANGE        (LEDRAW_RED_EN | LEDRAW_YELLOW_EN)
#define LEDRAW_NOTGREEN      (LEDRAW_BLUE_EN | LEDRAW_YELLOW_EN)

#define LEDRAW_BLUE_PULSE    (LEDRAW_BLUE | LEDRAW_BLUE_PULSE_EN)
#define LEDRAW_PURPLE_PULSE  (LEDRAW_PURPLE | LEDRAW_BLUE_PULSE_EN | LEDRAW_RED_PULSE_EN)
#define LEDRAW_ORANGE_PULSE   (LEDRAW_ORANGE | LEDRAW_RED_PULSE_EN | LEDRAW_YELLOW_PULSE_EN)

#define SMCREG_NOTIFICATION_LED (0x44)

// Writable mask: 0x07030E00 (CTRL0_POFFLG_FPOFF,CTRL0_POFFLG_4S,CTRL0_POFFLG_TMR,CTRL0_PONFLG_SYS,CTRL0_PONLG_TMR, CTRL0_UNSTBL_PWR,CTRL0_LOW_BATT,CTRL0_FLG_00000400)
#define CTRL0_CANARY       (0x00000100) // Used by IOS-ACP when handling the rtcflag0.dat and rtcflag1.dat files.
#define CTRL0_LOW_BATT     (0x00000200) // Set when the CMOS battery is worn off.
#define CTRL0_FLG_00000400 (0x00000400) // Set when the CMOS battery is worn off.
#define CTRL0_UNSTBL_PWR   (0x00000800) // Unknown. boot0 checks this before launching a recovery boot1 image.
#define CTRL0_PONLG_TMR    (0x00010000) // Set upon a ON timer power on.
#define CTRL0_PONFLG_SYS   (0x00020000) // Set upon a system power event.
#define CTRL0_FLG_00400000 (0x00400000)
#define CTRL0_FLG_00800000 (0x00800000)
#define CTRL0_POFFLG_TMR   (0x01000000) // Set upon a OFF timer power off.
#define CTRL0_POFFLG_4S    (0x02000000) // Set upon a 4 second power button press.
#define CTRL0_POFFLG_FPOFF (0x04000000) // Set upon a forced power off.

#define SMC_SHUTDOWN_POFF            (0)
#define SMC_SHUTDOWN_RESET           (1)
#define SMC_SHUTDOWN_RESET_NO_DEFUSE (2)

int smc_read_register(u8 offset, u8* data);
int smc_write_register(u8 offset, u8 data);
int smc_write_register_multiple(u8 offset, u8* data, u32 count);
int smc_mask_register(u8 offset, u8 mask, u8 val);

int smc_write_raw(u8 data);
int smc_write_raw_multiple(u8* data, u32 count);

u32 smc_get_ctrl1();
void smc_set_ctrl1(u32 val);
u32 smc_get_ctrl0();
void smc_set_ctrl0(u32 val);
u8 smc_get_events(void);
u8 smc_wait_events(u8 mask);

int smc_set_notification_led(u8 val);
int smc_set_odd_power(bool enable);
int smc_eject_request();
int smc_set_cc_indicator(int state);
int smc_set_on_indicator(int state);
int smc_set_off_indicator(int state);
int smc_bt_rst();
int smc_wifi_rst();
int smc_drc_wifi_rst();

int smc_set_rearusb_power(int state);
int smc_set_frontusb_power(int state);
int smc_set_wifi_idk(int state);

void smc_power_off(void);
void smc_reset(void);
void smc_reset_no_defuse(void);

void smc_get_panic_reason(char* buffer);
void smc_set_panic_reason(const char* buffer);

#endif
