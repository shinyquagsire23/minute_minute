/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"

static inline u32 _byteswap_ulong(u32 val)
{
    u8 val_3 = (val >> 24) & 0xFF;
    u8 val_2 = (val >> 16) & 0xFF;
    u8 val_1 = (val >> 8) & 0xFF;
    u8 val_0 = val & 0xFF;

    return (val_0 << 24) | (val_1 << 16) | (val_2 << 8) | (val_3);
}

static inline u16 _byteswap_ushort(u16 val)
{
    u8 val_hi = val >> 8;
    u8 val_lo = val & 0xFF;

    return (val_lo << 8) | val_hi;
}

static inline u32 read32_unaligned(const u8* pData)
{
    u8 val_3 = pData[3];
    u8 val_2 = pData[2];
    u8 val_1 = pData[1];
    u8 val_0 = pData[0];

    return (val_0 << 24) | (val_1 << 16) | (val_2 << 8) | (val_3);
}

static inline u32 read32le_unaligned(u8* pData)
{
    u8 val_3 = pData[0];
    u8 val_2 = pData[1];
    u8 val_1 = pData[2];
    u8 val_0 = pData[3];

    return (val_0 << 24) | (val_1 << 16) | (val_2 << 8) | (val_3);
}

static inline u32 read32(u32 addr)
{
    u32 data;
    __asm__ volatile ("ldr\t%0, [%1]" : "=l" (data) : "l" (addr));
    return data;
}

static inline void write32(u32 addr, u32 data)
{
    __asm__ volatile ("str\t%0, [%1]" : : "l" (data), "l" (addr));
}

static inline u32 set32(u32 addr, u32 set)
{
    u32 data;
    __asm__ volatile (
        "ldr\t%0, [%1]\n"
        "\torr\t%0, %2\n"
        "\tstr\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set)
    );
    return data;
}

static inline u32 clear32(u32 addr, u32 clear)
{
    u32 data;
    __asm__ volatile (
        "ldr\t%0, [%1]\n"
        "\tbic\t%0, %2\n"
        "\tstr\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (clear)
    );
    return data;
}


static inline u32 mask32(u32 addr, u32 clear, u32 set)
{
    u32 data;
    __asm__ volatile (
        "ldr\t%0, [%1]\n"
        "\tbic\t%0, %3\n"
        "\torr\t%0, %2\n"
        "\tstr\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set), "l" (clear)
    );
    return data;
}

static inline u16 read16(u32 addr)
{
    u32 data;
    __asm__ volatile ("ldrh\t%0, [%1]" : "=l" (data) : "l" (addr));
    return data;
}

static inline void write16(u32 addr, u16 data)
{
    __asm__ volatile ("strh\t%0, [%1]" : : "l" (data), "l" (addr));
}

static inline u16 set16(u32 addr, u16 set)
{
    u16 data;
    __asm__ volatile (
        "ldrh\t%0, [%1]\n"
        "\torr\t%0, %2\n"
        "\tstrh\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set)

    );
    return data;
}

static inline u16 clear16(u32 addr, u16 clear)
{
    u16 data;
    __asm__ volatile (
        "ldrh\t%0, [%1]\n"
        "\tbic\t%0, %2\n"
        "\tstrh\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (clear)
    );
    return data;
}


static inline u16 mask16(u32 addr, u16 clear, u16 set)
{
    u16 data;
    __asm__ volatile (
        "ldrh\t%0, [%1]\n"
        "\tbic\t%0, %3\n"
        "\torr\t%0, %2\n"
        "\tstrh\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set), "l" (clear)
    );
    return data;
}

static inline u8 read8(u32 addr)
{
    u32 data;
    __asm__ volatile ("ldrb\t%0, [%1]" : "=l" (data) : "l" (addr));
    return data;
}

static inline void write8(u8 *addr, u8 data)
{
    __asm__ volatile ("strb\t%0, [%1]" : : "l" (data), "l" (addr));
}

static inline u8 set8(u32 addr, u8 set)
{
    u8 data;
    __asm__ volatile (
        "ldrb\t%0, [%1]\n"
        "\torr\t%0, %2\n"
        "\tstrb\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set)
    );
    return data;
}

static inline u8 clear8(u32 addr, u8 clear)
{
    u8 data;
    __asm__ volatile (
        "ldrb\t%0, [%1]\n"
        "\tbic\t%0, %2\n"
        "\tstrb\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (clear)
    );
    return data;
}

static inline u8 mask8(u32 addr, u8 clear, u8 set)
{
    u8 data;
    __asm__ volatile (
        "ldrb\t%0, [%1]\n"
        "\tbic\t%0, %3\n"
        "\torr\t%0, %2\n"
        "\tstrb\t%0, [%1]"
        : "=&l" (data)
        : "l" (addr), "l" (set), "l" (clear)
    );
    return data;
}

/*
 * These functions are guaranteed to copy by reading from src and writing to dst in <n>-bit units
 * If size is not aligned, the remaining bytes are not copied
 */
void memset32(void *dst, u32 value, u32 size);
void memcpy32(void *dst, void *src, u32 size);
void memset16(void *dst, u16 value, u32 size);
void memcpy16(void *dst, void *src, u32 size);
void memset8(void *dst, u8 value, u32 size);
void memcpy8(void *dst, void *src, u32 size);

void hexdump(const void *d, int len);
void udelay(u32 d);
void panic(u8 v);

static inline u32 get_cpsr(void)
{
    u32 data;
    __asm__ volatile ( "mrs\t%0, cpsr" : "=r" (data) );
    return data;
}

#define STACK_ALIGN(type, name, cnt, alignment)         \
u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) + \
(((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) - \
((sizeof(type)*(cnt))%(alignment))) : 0))]; \
type *name = (type*)(((u32)(_al__##name)) + ((alignment) - (( \
(u32)(_al__##name))&((alignment)-1))))


#define PTR_OFFS(a, b) ((void*)(((intptr_t)a) + b))

#define ALIGN_FORWARD(x,align) \
    ((__typeof__(x))((((u32)(x)) + (align) - 1) & (~(align-1))))

#define ALIGN_BACKWARD(x,align) \
    ((__typeof__(x))(((u32)(x)) & (~(align-1))))

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; })

#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; })

#define BIT(n) (1<<n)

#endif
