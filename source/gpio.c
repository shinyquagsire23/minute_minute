/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "gpio.h"
#include "latte.h"
#include "utils.h"
#include <string.h>

void gpio_set(u16 gpio_id, u8 val)
{
    clear32(LT_GPIO_OWNER, BIT(gpio_id));

    mask32(LT_GPIO_OUT, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
    mask32(LT_GPIOE_OUT, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
}

void gpio_enable(u16 gpio_id, u8 val)
{
    clear32(LT_GPIO_OWNER, BIT(gpio_id));

    clear32(LT_GPIO_INTLVL, BIT(gpio_id));
    clear32(LT_GPIOE_INTLVL, BIT(gpio_id));
    clear32(LT_GPIO_INTMASK, BIT(gpio_id));
    clear32(LT_GPIOE_INTMASK, BIT(gpio_id));
    mask32(LT_GPIO_ENABLE, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
}

void gpio_set_dir(u16 gpio_id, u8 dir)
{
    clear32(LT_GPIO_OWNER, BIT(gpio_id));

    set32(LT_GPIOE_DIR, dir ? BIT(gpio_id) : 0);
    set32(LT_GPIO_DIR, dir ? BIT(gpio_id) : 0);
}

void gpio2_set(u16 gpio_id, u8 val)
{
    clear32(LT_GPIO2_OWNER, BIT(gpio_id));

    mask32(LT_GPIO2_OUT, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
    mask32(LT_GPIOE2_OUT, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
}

void gpio2_enable(u16 gpio_id, u8 val)
{
    clear32(LT_GPIO2_OWNER, BIT(gpio_id));

    clear32(LT_GPIO2_INTLVL, BIT(gpio_id));
    clear32(LT_GPIOE2_INTLVL, BIT(gpio_id));
    clear32(LT_GPIO2_INTMASK, BIT(gpio_id));
    clear32(LT_GPIOE2_INTMASK, BIT(gpio_id));
    mask32(LT_GPIO2_ENABLE, BIT(gpio_id), (val ? BIT(gpio_id) : 0));
}

void gpio2_set_dir(u16 gpio_id, u8 dir)
{
    clear32(LT_GPIO2_OWNER, BIT(gpio_id));

    set32(LT_GPIOE2_DIR, dir ? BIT(gpio_id) : 0);
    set32(LT_GPIO2_DIR, dir ? BIT(gpio_id) : 0);
}

void gpio_dcdc_pwrcnt2_set(u8 val)
{
    gpio2_set_dir(GP2_DCDC2, GPIO_DIR_OUT);
    gpio2_set(GP2_DCDC2, val);
    gpio2_enable(GP2_DCDC2, 1);
}

void gpio_dcdc_pwrcnt_set(u8 val)
{
    gpio_set_dir(GP_DCDC, GPIO_DIR_OUT);
    gpio_set(GP_DCDC, val);
    gpio_enable(GP_DCDC, 1);
}

void gpio_fan_set(u8 val)
{
    gpio_set_dir(GP_FAN, GPIO_DIR_OUT);
    gpio_set(GP_FAN, val);
    gpio_enable(GP_FAN, 1);
}

void gpio_fanspeed_set(bool fullspeed){
    gpio2_set_dir(GP2_FANSPEED, GPIO_DIR_OUT);
    gpio2_set(GP2_FANSPEED, fullspeed);
    gpio2_enable(GP2_FANSPEED, 1);
}

void gpio_smc_i2c_init()
{
    gpio2_enable(GP2_SMC_I2C_CLK, 1);
    gpio2_enable(GP2_SMC_I2C_DAT, 1);
}

void gpio_ave_i2c_init()
{
    gpio_enable(GP_AVE_SCL, 1);
    gpio_enable(GP_AVE_SDA, 1);
    gpio_enable(GP_AV1_I2C_CLK, 1);
    gpio_enable(GP_AV1_I2C_DAT, 1);
    gpio2_basic_set(GP2_AVRESET, 1);
    gpio2_basic_set(GP2_AVRESET, 0);
    gpio2_basic_set(GP2_AVRESET, 1);
}

void gpio_basic_set(u16 gpio_id, u8 val)
{
    gpio_set_dir(gpio_id, GPIO_DIR_OUT);
    gpio_set(gpio_id, val);
    gpio_enable(gpio_id, 1);
}

void gpio2_basic_set(u16 gpio_id, u8 val)
{
    gpio2_set_dir(gpio_id, GPIO_DIR_OUT);
    gpio2_set(gpio_id, val);
    gpio2_enable(gpio_id, 1);
}

void gpio_debug_send(u8 val)
{
    clear32(LT_GPIO_OWNER, GP_DEBUG_MASK);
    set32(LT_GPIO_ENABLE, GP_DEBUG_MASK); // Enable
    set32(LT_GPIO_DIR, GP_DEBUG_MASK); // Direction = Out

    mask32(LT_GPIO_OUT, GP_DEBUG_MASK, (val << GP_DEBUG_SHIFT));
    udelay(1);
}

void gpio_debug_serial_send(u8 val)
{
    clear32(LT_GPIO_OWNER, GP_DEBUG_MASK);
    set32(LT_GPIO_ENABLE, GP_DEBUG_MASK); // Enable
    set32(LT_GPIO_DIR, GP_DEBUG_SERIAL_MASK); // Direction = Out

    mask32(LT_GPIO_OUT, GP_DEBUG_SERIAL_MASK, (val << GP_DEBUG_SHIFT));
}

u8 gpio_debug_serial_read()
{
    clear32(LT_GPIO_OWNER, GP_DEBUG_MASK);
    set32(LT_GPIO_ENABLE, GP_DEBUG_MASK); // Enable
    set32(LT_GPIO_DIR, GP_DEBUG_SERIAL_MASK); // Direction = Out

    return (read32(LT_GPIO_IN) >> (GP_DEBUG_SHIFT+6)) & 1;
}
