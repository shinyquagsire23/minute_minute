/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __RTC_H__
#define __RTC_H__

#include "types.h"

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

#define CTRL1_POFF_EXE     (0x00010000)
#define CTRL1_DEVPWR_SYNC  (0x00000400)
#define CTRL1_SLEEP_EN     (0x00000100)
#define CTRL1_FPOFF_MODE   (0x00000008)
#define CTRL1_CLKOUT_EN    (0x00000004)
#define CTRL1_4COUNT_EN    (0x00000001)

#define PFLAGS_INVALID              (0x00000001)
#define PFLAG_PON_WAKEREQ1_EVENT_SW (0x00000002)
#define PFLAG_PON_WAKEBT_EVENT_SW   (0x00000004)
#define PFLAG_PON_POWER_BTN_SW      (0x00000008)
#define PFLAG_ENTER_BG_NORMAL_MODE  (0x00000010)
#define PFLAG_PON_SMC_DISC          (0x00000200)
#define PFLAG_PON_SYNC_BTN          (0x00000400)
#define PFLAG_PON_RESTART           (0x00001000)
#define PFLAG_PON_RELOAD            (0x00002000)
#define PFLAG_PON_COLDBOOT          (0x00010000)
#define PON_SMC_TIMER               (0x00020000)
#define PFLAG_40000                 (0x00040000)
#define CMPT_RETSTAT1               (0x00080000)
#define CMPT_RETSTAT0               (0x00100000)
#define PFLAG_DDR_SREFRESH          (0x00200000)
#define POFF_TMR                    (0x00400000)
#define POFF_4S                     (0x00800000)
#define POFF_FORCED                 (0x01000000)
#define PON_TMR                     (0x02000000)
#define PON_SMC                     (0x04000000)
#define PON_WAKEREQ1_EVENT          (0x08000000)
#define PON_WAKEREQ0_EVENT          (0x10000000)
#define PON_WAKEBT_EVENT            (0x20000000)
#define PON_EJECT_BTN               (0x40000000)
#define PON_POWER_BTN               (0x80000000)

u32 rtc_get_ctrl1();
void rtc_set_ctrl1(u32 val);
u32 rtc_get_ctrl0();
void rtc_set_ctrl0(u32 val);
void rtc_get_panic_reason(char* buffer);
void rtc_set_panic_reason(const char* buffer);

#endif // __RTC_H__