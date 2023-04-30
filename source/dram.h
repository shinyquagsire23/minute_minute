/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _DRAM_H
#define _DRAM_H

#include "types.h"

typedef struct bsp_system_clock_info
{
    u32 systemClockFrequency;
    u32 timerFrequency;
} bsp_system_clock_info;

#define DDR_SEQ_BL4         (0x00000000)
#define DDR_SEQ_TRCDR       (0x00000001)
#define DDR_SEQ_TRCDW       (0x00000002)
#define DDR_SEQ_TRAS        (0x00000003)
#define DDR_SEQ_TRC         (0x00000004)
#define DDR_SEQ_TCL         (0x00000005)
#define DDR_SEQ_TWL         (0x00000006)
#define DDR_SEQ_RRL         (0x00000007)
#define DDR_SEQ_TRRD        (0x00000008)
#define DDR_SEQ_TFAW        (0x00000009)
#define DDR_SEQ_TRFC        (0x0000000a)
#define DDR_SEQ_TRDWR       (0x0000000b)
#define DDR_SEQ_TWRRD       (0x0000000c)
#define DDR_SEQ_TR2R        (0x0000000d)
#define DDR_SEQ_RDPR        (0x0000000e)
#define DDR_SEQ_WRPR        (0x0000000f)
#define DDR_SEQ_BANK4       (0x00000010)
#define DDR_SEQ_QSOE0       (0x00000011)
#define DDR_SEQ_QSOE1       (0x00000012)
#define DDR_SEQ_QSOE2       (0x00000013)
#define DDR_SEQ_QSOE3       (0x00000014)
#define DDR_SEQ_RANK2       (0x00000015)
#define DDR_SEQ_DDR2        (0x00000016)
#define DDR_SEQ_RSTB        (0x00000017)
#define DDR_SEQ_CKEEN       (0x00000018)
#define DDR_SEQ_CKEDYN      (0x00000019)
#define DDR_SEQ_CKESR       (0x0000001a)
#define DDR_SEQ_ODTON       (0x0000001b)
#define DDR_SEQ_ODTDYN      (0x0000001c)
#define DDR_SEQ_ODT0        (0x0000001d)
#define DDR_SEQ_ODT1        (0x0000001e)
#define DDR_SEQ_RECEN0      (0x0000001f)
#define DDR_SEQ_RECEN1      (0x00000020)
#define DDR_SEQ_IDLEST      (0x00000021)
#define DDR_SEQ_NPLRD       (0x00000022)
#define DDR_SEQ_NPLCONF     (0x00000023)
#define DDR_SEQ_NOOPEN      (0x00000024)
#define DDR_SEQ_QSDEF       (0x00000025)
#define DDR_SEQ_ODTPIN      (0x00000026)
#define DDR_SEQ_NPLDLY      (0x00000027)
#define DDR_SEQ_STATUS      (0x00000028)
#define DDR_SEQ_VENDORID0   (0x00000029)
#define DDR_SEQ_VENDORID1   (0x0000002a)
#define DDR_SEQ_NMOSPD      (0x0000002b)
#define DDR_SEQ_STR0        (0x0000002c)
#define DDR_SEQ_STR1        (0x0000002d)
#define DDR_SEQ_STR2        (0x0000002e)
#define DDR_SEQ_STR3        (0x0000002f)
#define DDR_SEQ_APAD0       (0x00000030)
#define DDR_SEQ_APAD1       (0x00000031)
#define DDR_SEQ_CKPAD0      (0x00000032)
#define DDR_SEQ_CKPAD1      (0x00000033)
#define DDR_SEQ_CMDPAD0     (0x00000034)
#define DDR_SEQ_CMDPAD1     (0x00000035)
#define DDR_SEQ_DQPAD0      (0x00000036)
#define DDR_SEQ_DQPAD1      (0x00000037)
#define DDR_SEQ_QSPAD0      (0x00000038)
#define DDR_SEQ_QSPAD1      (0x00000039)
#define DDR_SEQ_WRDQ0       (0x0000003a)
#define DDR_SEQ_WRDQ1       (0x0000003b)
#define DDR_SEQ_WRQS0       (0x0000003c)
#define DDR_SEQ_WRQS1       (0x0000003d)
#define DDR_SEQ_MADJL       (0x0000003e)
#define DDR_SEQ_MADJH       (0x0000003f)
#define DDR_SEQ_SADJ0L      (0x00000040)
#define DDR_SEQ_SADJ0H      (0x00000041)
#define DDR_SEQ_SADJ1L      (0x00000042)
#define DDR_SEQ_SADJ1H      (0x00000043)
#define DDR_SEQ_RDDQ1       (0x00000044)
#define DDR_SEQ_WR          (0x00000045)
#define DDR_SEQ_PADA        (0x00000046)
#define DDR_SEQ_PAD0        (0x00000047)
#define DDR_SEQ_PAD1        (0x00000048)
#define DDR_SEQ_ARAM        (0x00000049)
#define DDR_SEQ_WR2PR       (0x0000004a)
#define DDR_SEQ_SYNC        (0x0000004b)
#define DDR_SEQ_RECVON      (0x0000004c)

#define CPL_CLK_V_LSB (0x10)
#define CPL_CLK_V_MSB (0x12)
#define CPL_CLK_S     (0x14)
#define CPL_BW_ADJ    (0x16)
#define CPL_CLK_F_LSB (0x18)
#define CPL_CLK_O_0DIV__CLK_R (0x1C)

#define DRAM_MODE_1         (0x01)
#define DRAM_MODE_CCBOOT    (0x02)
#define DRAM_MODE_4         (0x04)
#define DRAM_MODE_SREFRESH  (0x08)
#define DRAM_MODE_10        (0x10)
#define DRAM_MODE_20        (0x20)
#define DRAM_MODE_40        (0x40)
#define DRAM_MODE_80        (0x80)

int mem_clocks_related_3(u16 mode);
int init_mem2(u16 mem_mode);

#endif // _DRAM_H