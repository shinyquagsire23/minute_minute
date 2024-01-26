/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#include "types.h"

enum {
    GP_POWER        = 0,
    GP_SHUTDOWN     = 1,
    GP_FAN          = 2,
    GP_DCDC         = 3,
    GP_DISPIN       = 4,
    GP_SLOTLED      = 5,
    GP_EJECTBTN     = 6,
    GP_SLOTIN       = 7,
    GP_SENSORBAR    = 8,
    GP_DOEJECT      = 9,
    GP_EEP_CS       = 10,
    GP_EEP_CLK      = 11,
    GP_EEP_MOSI     = 12,
    GP_EEP_MISO     = 13,
    GP_AVE_SCL      = 14,
    GP_AVE_SDA      = 15,
    GP_DEBUG0       = 16,
    GP_DEBUG1       = 17,
    GP_DEBUG2       = 18,
    GP_DEBUG3       = 19,
    GP_DEBUG4       = 20,
    GP_DEBUG5       = 21,
    GP_DEBUG6       = 22,
    GP_DEBUG7       = 23,
    GP_AV1_I2C_CLK  = 24,
    GP_AV1_I2C_DAT  = 25,
};

enum {
    GP2_FANSPEED        = 0,
    GP2_SMC_I2C_CLK     = 1,
    GP2_SMC_I2C_DAT     = 2,
    GP2_DCDC2           = 3,
    GP2_AVINTERRUPT     = 4,
    GP2_CCRIO12         = 5,
    GP2_AVRESET         = 6,
};

#define GPIO_DIR_IN  (0)
#define GPIO_DIR_OUT (1)

#define GP_DEBUG_SHIFT 16
#define GP_DEBUG_MASK 0xFF0000

#define GP_DEBUG_SERIAL_MASK 0xBF0000 // bit1 is input

#define GP2_SMC (BIT(GP2_SMC_I2C_CLK)|BIT(GP2_SMC_I2C_DAT))

#define GP_ALL 0xFFFFFF
#define GP_OWNER_PPC (GP_AVE_SDA | GP_AVE_SCL | GP_DOEJECT | GP_SENSORBAR | GP_SLOTIN | GP_SLOTLED)
#define GP_OWNER_ARM (GP_ALL ^ GP_OWNER_PPC)
#define GP_INPUTS (GP_POWER | GP_EJECTBTN | GP_SLOTIN | GP_EEP_MISO | GP_AVE_SDA)
#define GP_OUTPUTS (GP_ALL ^ GP_INPUTS)
#define GP_ARM_INPUTS (GP_INPUTS & GP_OWNER_ARM)
#define GP_PPC_INPUTS (GP_INPUTS & GP_OWNER_PPC)
#define GP_ARM_OUTPUTS (GP_OUTPUTS & GP_OWNER_ARM)
#define GP_PPC_OUTPUTS (GP_OUTPUTS & GP_OWNER_PPC)
#define GP_DEFAULT_ON (GP_AVE_SCL | GP_DCDC | GP_FAN)
#define GP_ARM_DEFAULT_ON (GP_DEFAULT_ON & GP_OWNER_ARM)
#define GP_PPC_DEFAULT_ON (GP_DEFAULT_ON & GP_OWNER_PPC)

#define FANSPEED_FAST true
#define FANSPEED_SLOW false

#ifdef MINUTE_BOOT1
#define SERIAL_DELAY (10)
#else
#define SERIAL_DELAY (1)
#endif

extern u8 test_read_serial;

void gpio_enable(u16 gpio_id, u8 val);
void gpio_set_dir(u16 gpio_id, u8 dir);

void gpio_dcdc_pwrcnt2_set(u8 val);
void gpio_dcdc_pwrcnt_set(u8 val);
void gpio_fan_set(u8 val);
void gpio_fanspeed_set(bool fullspeed);
void gpio_smc_i2c_init();
void gpio_ave_i2c_init();
void gpio_basic_set(u16 gpio_id, u8 val);
void gpio2_basic_set(u16 gpio_id, u8 val);
void gpio_debug_send(u8 val);
void gpio_debug_serial_send(u8 val);
u8 gpio_debug_serial_read();

#endif

