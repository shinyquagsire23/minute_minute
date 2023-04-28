/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef _I2C_H
#define _I2C_H

#include "types.h"

#define I2C_SLAVE_SMC (0x50)

void i2c_init(u32 clock, u32 channel);
int i2c_read(u8 slave_7bit, u8* data, size_t size);
int i2c_write(u8 slave_7bit, const u8* data, size_t size);

void ave_i2c_init(u32 clock, u32 channel);
int ave_i2c_read(u8 slave_7bit, u8* data, size_t size);
int ave_i2c_write(u8 slave_7bit, const u8* data, size_t size);

#endif
