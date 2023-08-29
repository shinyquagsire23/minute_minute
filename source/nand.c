/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "latte.h"
#include "nand.h"
#include "utils.h"
#include "string.h"
#include "memory.h"
#include "crypto.h"
#include "irq.h"
#include "gfx.h"
#include "types.h"

//#define NAND_DEBUG  1
#define NAND_SUPPORT_WRITE 1
#define NAND_SUPPORT_ERASE 1

#ifdef NAND_DEBUG
#   define  NAND_debug(f, arg...) printf("NAND: " f, ##arg);
#else
#   define  NAND_debug(f, arg...)
#endif

#define NAND_RESET      0xff
#define NAND_CHIPID     0x90
#define NAND_GETSTATUS  0x70
#define NAND_ERASE_PRE  0x60
#define NAND_ERASE_POST 0xd0
#define NAND_READ_PRE   0x00
#define NAND_READ_POST  0x30
#define NAND_WRITE_PRE  0x80
#define NAND_WRITE_POST 0x10
#define NAND_RANDOMDATA_IN 0x85

#define NAND_BUSY_MASK  0x80000000
#define NAND_ERROR      0x20000000

#define NAND_FLAGS_IRQ  0x40000000
#define NAND_FLAGS_WAIT 0x8000
#define NAND_FLAGS_WR   0x4000
#define NAND_FLAGS_RD   0x2000
#define NAND_FLAGS_ECC  0x1000

#define CTRL_CMD(cmd)       (0x00ff0000 & (cmd << 16))
#define CTRL_ADDR(addr)     (0x1f000000 & (addr << 24))
#define CTRL_SIZE(size)     (0x00000fff & (size))

/* ECC definitions */
#define ECC_SIZE            0x10
#define ECC_STOR_OFFS       0x30
#define ECC_CALC_OFFS       0x40

static u32 initialized = 0;
static volatile int irq_flag;
static u32 last_page_read = 0;
#if defined(NAND_SUPPORT_ERASE) || defined(NAND_SUPPORT_WRITE)
static u32 nand_min_page = 0x200; // default to protecting boot1+boot2
static u8 nand_status_buf[STATUS_BUF_SIZE] ALIGNED(NAND_DATA_ALIGN);
static u8 nand_spare_buf[96] ALIGNED(NAND_DATA_ALIGN);
#endif

void nand_irq(void)
{
    //int code, tag, err = 0;
    if(read32(NAND_CTRL) & NAND_ERROR) {
        printf("NAND: Error on IRQ\n");
        //err = -1;
    }
    ahb_flush_from(WB_FLA);
    ahb_flush_to(RB_IOD);

    irq_flag = 1;
}

static void __nand_wait(void) {
    NAND_debug("waiting...\n");
    while(read32(NAND_CTRL) & NAND_BUSY_MASK);
    NAND_debug("wait done\n");
    if(read32(NAND_CTRL) & NAND_ERROR)
        printf("NAND: Error on wait\n");
    ahb_flush_from(WB_FLA);
    ahb_flush_to(RB_IOD);
}

void nand_send_command(u32 command, u32 bitmask, u32 flags, u32 num_bytes) {
    u32 cmd = NAND_BUSY_MASK | (bitmask << 24) | (command << 16) | flags | num_bytes;

    NAND_debug("nand_send_command(%x, %x, %x, %x) -> %x\n",
        command, bitmask, flags, num_bytes, cmd);

    write32(NAND_CTRL, 0x7fffffff);
    write32(NAND_CTRL, 0);
    write32(NAND_CTRL, cmd);
}

void __nand_set_address(s32 page_off, s32 pageno) {
    if (page_off != -1) write32(NAND_ADDR0, page_off);
    if (pageno != -1)   write32(NAND_ADDR1, pageno);
}

void __nand_setup_dma(u8 *data, u8 *spare) {
    if ((u32)data & (NAND_DATA_ALIGN - 1)) {
        printf("NAND: page buffer 0x%08x is not aligned, data will be corrupted\n", data);
    }

    if (((s32)data) != -1) {
        write32(NAND_DATA, dma_addr(data));
    }
    if (((s32)spare) != -1) {
        u32 addr = dma_addr(spare);
        if(addr & 0x7f)
            printf("NAND: Spare buffer 0x%08x is not aligned, data will be corrupted\n", addr);
        write32(NAND_ECC, addr);
    }
}

int nand_reset(u32 bank) {
    NAND_debug("nand_reset()\n");

    write32(NAND_CTRL, 0);
    while(read32(NAND_CTRL) & NAND_CMD_EXEC);
    write32(NAND_CTRL, 0);
    write32(NAND_CONF, 0x743e3eff);
    write32(NAND_BANK, 0x00000001);

    write32(NAND_CONF, 0xcb3e0e7f);

    write32(NAND_UNK2, 0);
    while((s32)read32(NAND_UNK2) < 0);
    write32(NAND_UNK2, 0);

    u32 base = NAND_UNK3;
    do
    {
        write32(base+0x00, 0);
        write32(base+0x04, 0);
        write32(base+0x08, 0);
        write32(base+0x0C, 0);
        write32(base+0x10, 0);
        write32(base+0x14, 0);
        base += 0x18;
    } while(base != NAND_UNK3 + 0xC0);

    write32(NAND_CTRL, 0);
    while(read32(NAND_CTRL) & NAND_CMD_EXEC);
    write32(NAND_CTRL, 0);

    write32(NAND_BANK, bank);
// IOS actually uses NAND_FLAGS_IRQ | NAND_FLAGS_WAIT here
    nand_send_command(NAND_RESET, 0, NAND_FLAGS_WAIT, 0);
    __nand_wait();
// enable NAND controller
    write32(NAND_CONF, 0x08000000);
// set configuration parameters for 512MB flash chips
    write32(NAND_CONF, 0x7c3e3eff);
    return 0;
}

void nand_get_id(u8 *idbuf) {
    irq_flag = 0;
    __nand_set_address(0,0);

    dc_invalidaterange(idbuf, 0x40);

    __nand_setup_dma(idbuf, (u8 *)-1);
    nand_send_command(NAND_CHIPID, 1, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);
}

void nand_get_status(u8 *status_buf) {
    irq_flag = 0;
    status_buf[0]=0;

    dc_invalidaterange(status_buf, 0x40);

    __nand_setup_dma(status_buf, (u8 *)-1);
    nand_send_command(NAND_GETSTATUS, 0, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);

    while(read32(NAND_CTRL) & NAND_CMD_EXEC);

    ahb_flush_from(WB_FLA);
    dc_invalidaterange(nand_status_buf, STATUS_BUF_SIZE);    
}

#if defined(NAND_SUPPORT_ERASE) || defined(NAND_SUPPORT_WRITE)
static int nand_check_error(void){
    /* get status */
    *nand_status_buf = 1;
    dc_flushrange(nand_status_buf, STATUS_BUF_SIZE);
    nand_get_status(nand_status_buf);
    return -(*nand_status_buf & 1);
}


#endif

void nand_wait(void) {
    if (!(read32(NAND_CTRL) & NAND_BUSY_MASK)) return;

// power-saving IRQ wait
    while(!irq_flag) {
        u32 cookie = irq_kill();
        if(!irq_flag)
            irq_wait();
        irq_restore(cookie);
    }
}

int nand_read_page(u32 pageno, void *data, void *ecc) {
    irq_flag = 0;
    last_page_read = pageno;  // needed for error reporting
    __nand_set_address(0, pageno);
    nand_send_command(NAND_READ_PRE, 0x1f, 0, 0);

    if (((s32)data) != -1) dc_invalidaterange(data, PAGE_SIZE);
    if (((s32)ecc) != -1)  dc_invalidaterange(ecc, ECC_BUFFER_SIZE);

    __nand_wait();
    __nand_setup_dma(data, ecc);
    nand_send_command(NAND_READ_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT | NAND_FLAGS_RD | NAND_FLAGS_ECC, 0x840);
    nand_wait();
    write32(NAND_CTRL, 0);
    ahb_flush_from(WB_FLA);
    dc_invalidaterange(data, PAGE_SIZE);
    dc_invalidaterange(ecc, ECC_BUFFER_ALLOC);
    if (read32(NAND_CTRL) & NAND_ERROR)
        return -1;
    return 0;
}

#ifdef NAND_SUPPORT_WRITE
int nand_write_page_raw(u32 pageno, void *data, void *ecc) {
    irq_flag = 0;
    NAND_debug("nand_write_page_raw(%u, %p, %p)\n", pageno, data, ecc);

#if 0
    // this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
    if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
        printf("Error: nand_write to page %d forbidden\n", pageno);
        return;
    }
#endif
    if (((s32)data) != -1) dc_flushrange(data, PAGE_SIZE + PAGE_SPARE_SIZE);
    if (((s32)ecc) != -1)  dc_flushrange(ecc, PAGE_SPARE_SIZE);
    ahb_flush_to(RB_FLA);
    __nand_set_address(0, pageno);
    __nand_setup_dma(data, ecc);
    nand_send_command(NAND_WRITE_PRE, 0x1f, NAND_FLAGS_WR, 0x840);
    __nand_wait();
    nand_send_command(NAND_WRITE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
    if(nand_check_error()){
        NAND_debug("nand_write_page(%d) failed\n", pageno);
        return -1;
    }
    return 0;
}

int nand_write_page(u32 pageno, void *data, void *spare) {
    irq_flag = 0;
    NAND_debug("nand_write_page(%u, %p, %p)\n", pageno, data, spare);

#if 0
    // this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
    if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
        printf("Error: nand_write to page %d forbidden\n", pageno);
        return -2;
    }
#endif
    if (((s32)data) != -1) dc_flushrange(data, PAGE_SIZE);
    ahb_flush_to(RB_FLA);
    dc_invalidaterange(nand_spare_buf + ECC_CALC_OFFS, ECC_SIZE);

    __nand_set_address(0, pageno);
    __nand_setup_dma(data, nand_spare_buf);
    nand_send_command(NAND_WRITE_PRE, 0x1f, NAND_FLAGS_WR | NAND_FLAGS_ECC, 0x800);
    __nand_wait();

    /* prepare page spare */
    ahb_flush_from(WB_FLA);
    if (spare) {
        memcpy(nand_spare_buf, spare, PAGE_SPARE_SIZE);
    } else {
        memset(nand_spare_buf, 0, PAGE_SPARE_SIZE);
    }
    nand_spare_buf[0] = 0xff;
    memcpy(nand_spare_buf + ECC_STOR_OFFS, nand_spare_buf + ECC_CALC_OFFS, ECC_SIZE);
    dc_flushrange(nand_spare_buf, PAGE_SPARE_SIZE);

    /* setup irq */
    write32(NAND_CTRL, 0);
    irq_flag = 0;
    irq_enable(IRQ_NAND);

    /* send spare content */
    write32(NAND_ADDR0, PAGE_SIZE);
    write32(NAND_ADDR1, 0);
    write32(NAND_DATA, dma_addr(nand_spare_buf));
    write32(NAND_ECC, 0);
    write32(NAND_CTRL,
        NAND_BUSY_MASK |
        CTRL_ADDR(0x3) |
        CTRL_CMD(NAND_RANDOMDATA_IN) |
        NAND_FLAGS_WR |
        CTRL_SIZE(PAGE_SPARE_SIZE));
    __nand_wait();

    /* program page*/
    nand_send_command(NAND_WRITE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
    nand_wait();
    if(nand_check_error()){
        NAND_debug("nand_write_page(%d) failed\n", pageno);
        return -1;
    }
    return 0;
}

#endif

#ifdef NAND_SUPPORT_ERASE
int nand_erase_block(u32 pageno) {
    irq_flag = 0;
    NAND_debug("nand_erase_block(%d)\n", pageno);

#if 0
    // this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
    if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
        printf("Error: nand_erase to page %d forbidden\n", pageno);
        return;
    }
#endif
    __nand_set_address(0, pageno);
    nand_send_command(NAND_ERASE_PRE, 0x1c, 0, 0);
    __nand_wait();
    nand_send_command(NAND_ERASE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
    if(nand_check_error()){
        NAND_debug("nand_erase_block(%d) failed\n", pageno);
        return -1;
    }
    NAND_debug("nand_erase_block(%d) done\n", pageno);
    return 0;
}
#endif

void nand_initialize(u32 bank)
{
    if(initialized == bank) return;

    irq_disable(IRQ_NAND);
    nand_reset(bank);
    irq_enable(IRQ_NAND);

    initialized = bank;
}

int nand_correct(u32 pageno, void *data, void *ecc)
{
    (void) pageno;

    u8 *dp = (u8*)data;
    u32 *ecc_read = (u32*)((u8*)ecc+0x30);
    u32 *ecc_calc = (u32*)((u8*)ecc+0x40);
    int i;
    int uncorrectable = 0;
    int corrected = 0;

    for(i=0;i<4;i++) {
        u32 syndrome = *ecc_read ^ *ecc_calc; //calculate ECC syncrome
        // don't try to correct unformatted pages (all FF)
        if ((*ecc_read != 0xFFFFFFFF) && syndrome) {
            if(!((syndrome-1)&syndrome)) {
                // single-bit error in ECC
                corrected++;
            } else {
                // byteswap and extract odd and even halves
                u16 even = (syndrome >> 24) | ((syndrome >> 8) & 0xf00);
                u16 odd = ((syndrome << 8) & 0xf00) | ((syndrome >> 8) & 0x0ff);
                if((even ^ odd) != 0xfff) {
                    // oops, can't fix this one
                    uncorrectable++;
                } else {
                    // fix the bad bit
                    dp[odd >> 3] ^= 1<<(odd&7);
                    corrected++;
                }
            }
        }
        dp += 0x200;
        ecc_read++;
        ecc_calc++;
    }
    if(uncorrectable || corrected)
        printf("ECC stats for NAND page 0x%lX: %d uncorrectable, %d corrected\n", pageno, uncorrectable, corrected);
    if(uncorrectable)
        return NAND_ECC_UNCORRECTABLE;
    if(corrected)
        return NAND_ECC_CORRECTED;
    return NAND_ECC_OK;
}

static u8 _nand_parity(u8 x)
{
    u8 y = 0;
    while (x)
    {
        y ^= (x & 1);
        x >>= 1;
    }
    return y;
}

void nand_create_ecc(void* in_data, void* spare_out)
{
    u8 a[12][2];
    u32 a0, a1;
    u8 x;

    u8* spare_buf = PTR_OFFS(spare_out, 0x0);
    memset(spare_buf, 0, 0x40);
    spare_buf[0] = 0xFF;

    u8* ecc = PTR_OFFS(spare_out, 0x30);
    const u8* data = (u8*)in_data;

    for (int k = 0; k < 4; k++)
    {
        memset(a, 0, sizeof(a));
        for (int i = 0; i < 0x200; i++)
        {
            x = data[i];
            for (int j = 0; j < 9; j++)
                a[3 + j][(i >> j) & 1] ^= x;
        }

        x = a[3][0] ^ a[3][1];
        a[0][0] = x & 0x55;
        a[0][1] = x & 0xaa;
        a[1][0] = x & 0x33;
        a[1][1] = x & 0xcc;
        a[2][0] = x & 0x0f;
        a[2][1] = x & 0xf0;

        for (int j = 0; j < 12; j++)
        {
            a[j][0] = _nand_parity(a[j][0]);
            a[j][1] = _nand_parity(a[j][1]);
        }
        a0 = a1 = 0;

        for (int j = 0; j < 12; j++)
        {
            a0 |= a[j][0] << j;
            a1 |= a[j][1] << j;
        }
        ecc[0] = a0;
        ecc[1] = a0 >> 8;
        ecc[2] = a1;
        ecc[3] = a1 >> 8;

        data += 512;
        ecc += 4;
    }
}
