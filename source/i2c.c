/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "types.h"
#include "utils.h"
#include "latte.h"
#include "gfx.h"
#include "gpio.h"

//#define I2C_DEBUG

void i2c_init(u32 clock, u32 channel)
{
    write32(LT_SMC_I2C_CLOCK, ((channel << 1) & 0xFFFF) | ((243000000 / 2) / clock << 16) | 1);
    write32(LT_SMC_I2C_INOUT_CTRL, 0);
}

void i2c_enable_int(u32 mask)
{
    write32(LT_SMC_I2C_INT_STATE, mask);
    set32(LT_SMC_I2C_INT_MASK, mask);
}

void i2c_disable_int(u32 mask)
{
    clear32(LT_SMC_I2C_INT_MASK, mask);
}

void i2c_inout_data(u8 data, bool last)
{
    u32 value = data | (last ? 1 << 8 : 0);
#ifdef I2C_DEBUG
    printf("i2c: writing value 0x%lx\n", value);
#endif
    write32(LT_SMC_I2C_INOUT_DATA, value);
    write32(LT_SMC_I2C_INOUT_CTRL, 1);
}

int i2c_wait_xfer_done(void)
{
    u32 mask = 0;

    for(int i = 0; i < 4999; i++)
    {
        mask = read32(LT_SMC_I2C_INT_STATE) & read32(LT_SMC_I2C_INT_MASK);
        if(mask & 0x1C)
        {
            clear32(LT_SMC_I2C_INT_STATE, ~read32(LT_SMC_I2C_INT_MASK));
#ifdef I2C_DEBUG
            printf("i2c: xfer error, mask 0x%lx!\n", mask);
#endif
            return -2;
        }
        if(mask & 3)
        {
            clear32(LT_SMC_I2C_INT_STATE, ~read32(LT_SMC_I2C_INT_MASK));
#ifdef I2C_DEBUG
            printf("i2c: xfer complete, mask 0x%lx\n", mask);
#endif
            return 0;
        }

        udelay(1000);
    }

#ifdef I2C_DEBUG
    printf("i2c: xfer fail, mask 0x%lx!\n", mask);
#endif
    write32(LT_SMC_I2C_INT_STATE, mask);
    return -1;
}

int i2c_write(u8 slave_7bit, const u8* data, size_t size)
{
    int res = 0;

    if(!data || size == 0 || size > 0x40)
        return -4;

    i2c_enable_int(0x1E);
    i2c_inout_data(slave_7bit << 1, 0);

    while(true) {
        if(size-- == 0) break;
        i2c_inout_data(*data++, size == 0);
    }

    res = i2c_wait_xfer_done();

    i2c_disable_int(0x1E);
    return res;
}

int i2c_read(u8 slave_7bit, u8* data, size_t size)
{
    int res = 0;

    if(!data || size == 0 || size > 0x40)
        return -4;

    i2c_enable_int(0x1D);
    i2c_inout_data((slave_7bit << 1) | 1, 0);

    size_t counter = size;
    while(true) {
        if(counter-- == 0) break;
        i2c_inout_data(0, counter == 0);
    }

    res = i2c_wait_xfer_done();
    if(res) {
        i2c_disable_int(0x1D);
        return res;
    }

    if(size > ((read32(LT_SMC_I2C_INOUT_SIZE) & 0xFF0000) >> 16)) {
#ifdef I2C_DEBUG
        printf("i2c: read size fail!\n");
#endif
        i2c_disable_int(0x1D);
        return -5;
    }

    size_t pos = 0;
    do
    {
        data[pos++] = read32(LT_SMC_I2C_INOUT_SIZE);
#ifdef I2C_DEBUG
        printf("i2c: read byte 0x%x @ %d\n", data[pos - 1], pos - 1);
#endif
    }
    while(pos != size);

    i2c_disable_int(0x1D);
    return 0;
}




void ave_i2c_init(u32 clock, u32 channel)
{
    gpio_ave_i2c_init();

    write32(LT_AVE_I2C_CLOCK, ((channel << 1) & 0xFFFF) | (((243000000 / 2) / clock) << 16) | 1);
    write32(LT_AVE_I2C_INOUT_CTRL, 0);
}

void ave_i2c_enable_int(u32 mask)
{
    write32(LT_AVE_I2C_INT_STATE, mask);
    set32(LT_AVE_I2C_INT_MASK, mask);
}

void ave_i2c_disable_int(u32 mask)
{
    clear32(LT_AVE_I2C_INT_MASK, mask);
}

void ave_i2c_inout_data(u8 data, bool last)
{
    u32 value = data | (last ? 1 << 8 : 0);
#ifdef I2C_DEBUG
    printf("i2c: writing value 0x%lx\n", value);
#endif
    write32(LT_AVE_I2C_INOUT_DATA, value);
    write32(LT_AVE_I2C_INOUT_CTRL, 1);
}

int ave_i2c_wait_xfer_done(void)
{
    u32 mask = 0;

    for(int i = 0; i < 4999; i++)
    {
        mask = read32(LT_AVE_I2C_INT_STATE) & read32(LT_AVE_I2C_INT_MASK);
        if(mask & 0xC80)
        {
            clear32(LT_AVE_I2C_INT_STATE, ~read32(LT_AVE_I2C_INT_MASK));
#ifdef I2C_DEBUG
            printf("i2c: xfer error, mask 0x%lx!\n", mask);
#endif
            return -2;
        }
        if(mask & 0x060)
        {
            clear32(LT_AVE_I2C_INT_STATE, ~read32(LT_AVE_I2C_INT_MASK));
#ifdef I2C_DEBUG
            printf("i2c: xfer complete, mask 0x%lx\n", mask);
#endif
            return 0;
        }

        udelay(1000);
    }

#ifdef I2C_DEBUG
    printf("i2c: xfer fail, mask 0x%lx!\n", mask);
#endif
    write32(LT_AVE_I2C_INT_STATE, mask);
    return -1;
}

int ave_i2c_write(u8 slave_7bit, const u8* data, size_t size)
{
    int res = 0;

    if(!data || size == 0 || size > 0x40)
        return -4;

    ave_i2c_enable_int(0xCC0);
#if 1
    ave_i2c_inout_data(slave_7bit << 1, 0);


    while(true) {
        if(size-- == 0) break;
        ave_i2c_inout_data(*data++, size == 0);
    }

    res = ave_i2c_wait_xfer_done();
#endif
    ave_i2c_disable_int(0xCC0);
    return res;
}

int ave_i2c_read(u8 slave_7bit, u8* data, size_t size)
{
    int res = 0;

    if(!data || size == 0 || size > 0x40)
        return -4;

    ave_i2c_enable_int(0xCE0);
    ave_i2c_inout_data((slave_7bit << 1) | 1, 0);

    size_t counter = size;
    while(true) {
        if(counter-- == 0) break;
        ave_i2c_inout_data(0, counter == 0);
    }

    res = ave_i2c_wait_xfer_done();
    if(res) {
        ave_i2c_disable_int(0xCE0);
        return res;
    }

    if(size > ((read32(LT_AVE_I2C_INOUT_SIZE) & 0xFF0000) >> 16)) {
#ifdef I2C_DEBUG
        printf("i2c: read size fail!\n");
#endif
        ave_i2c_disable_int(0xCE0);
        return -5;
    }

    size_t pos = 0;
    do
    {
        data[pos++] = read32(LT_AVE_I2C_INOUT_SIZE);
#ifdef I2C_DEBUG
        printf("i2c: read byte 0x%x @ %d\n", data[pos - 1], pos - 1);
#endif
    }
    while(pos != size);

    ave_i2c_disable_int(0xCE0);
    return 0;
}
