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
#include "memory.h"
#include "ancast.h"
#include "latte.h"
#include "utils.h"
#include "gfx.h"
#include "irq.h"
#include "crypto.h"
#include "rtc.h"
#include "exi.h"

#include <stdio.h>
#include <string.h>

#define ESPRESSO_EFUSE_ADDR     (0x0C320000)
#define ESPRESSO_BOOTROM_ADDR   (0x00000000)
#define DUMP_BLOCK_ADDR         (0x08200000)
#define DUMP_STUB_ADDR          (0x00004000)

#define EFUSE_BLOCK_LEN (0x40)
#define BOOTROM_LEN     (0x4000)

#define DUMP_BLOCK_SPR_OFFS      (0x20)
#define DUMP_BLOCK_EFUSE_OFFS    (DUMP_BLOCK_SPR_OFFS + 0x100)
#define DUMP_BLOCK_BOOTROM_OFFS  (DUMP_BLOCK_EFUSE_OFFS + EFUSE_BLOCK_LEN)
#define DUMP_BLOCK_END           (DUMP_BLOCK_BOOTROM_OFFS + BOOTROM_LEN)

// Holds HRESET, SRESET and PI reset
void ppc_hold_resets(void)
{
    clear32(LT_RESETS_COMPAT, RSTB_PI | RSTB_CPU | SRSTB_CPU);
    udelay(100);
}

// Does a full reset of the Espresso for Cafe mode.
void ppc_reset(void)
{
    ppc_hold_resets();

    mask32(LT_COMPAT_MEMCTRL_STATE, 0xFFE0022F, 0x08100000);

    clear32(LT_60XE_CFG, 8);
    mask32(LT_60XE_CFG, 0x1C0000, 0x20000);
    set32(LT_60XE_CFG, 0xC000);

    set32(LT_RESETS_COMPAT, RSTB_PI);
    clear32(LT_COMPAT_MEMCTRL_STATE, 0x20); // PPC clock mult on

    // Allow clocks to settle. This wait time is from IOSU.
    udelay(20000);

    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
}

// Holds PPC in reset, prepares strap pins (vWii vs Cafe vs factory),
// and upclocks PPC to Cafe (3x) speeds.
void ppc_prepare_compat_and_straps(u32 val)
{
    ppc_hold_resets();

    // - vWii uses | 0x26C. The lower nibble (0xC) gets sent into the upper
    //   nibble of the "SCR" SPR.
    // - vWii also seems to |= 0x400.
    // - Value 0x2 seems to be used on unfused/factory production units?
    //     IOS checks upper(ppcPVR) == 0x7001 and lower(ppcPVR) > 0x100 to
    //     not use value 0x2.
    mask32(LT_COMPAT_MEMCTRL_STATE, 0xFFE0022F, 0x08100000 | val);

    clear32(LT_60XE_CFG, 8);
    mask32(LT_60XE_CFG, 0x1C0000, 0x20000);

    // From IOSU
    if (BSP_HARDWARE_VERSION_GROUP(latte_get_hw_version()) <= BSP_HARDWARE_VERSION_GROUP_LATTE_A2X) {
        clear32(LT_60XE_CFG, 0xC000); // TODO: 
    }
    else {
        set32(LT_60XE_CFG, 0xC000); // TODO: 
    }

    set32(LT_RESETS_COMPAT, RSTB_PI);
    if (seeprom.bc.ppc_clock_multiplier)
        clear32(LT_COMPAT_MEMCTRL_STATE, 0x20); // PPC clock mult on
    else
        set32(LT_COMPAT_MEMCTRL_STATE, 0x20); // PPC clock mult off

    // Allow clocks to settle. This wait time is from IOSU.
    udelay(20000);
}

// Primes the PPC with two full, uninterrupted boots, in case
// the last bruteforce got it into a weird state.
void ppc_extra_safe_boot_pumping_for_bruteforcing()
{
    // Boot the PPC once fully, with lots of rest time.
    // Without an ancast, we expect this to fail.
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
    clear32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);

    // Boot it again just in case
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
    clear32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
}

// Asserts SRESET. Execution is triggered on the falling edge.
void ppc_do_sreset()
{
    clear32(LT_RESETS_COMPAT, SRSTB_CPU);
    udelay(100);
    set32(LT_RESETS_COMPAT, SRSTB_CPU);
}

// Release SRESET and HRESET at the same time.
void ppc_release_reset()
{
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
}

// This doesn't seem to work, unfortunately.
void ppc_do_defuse(int width)
{
    u32 val = read32(LT_RESETS_COMPAT) | RSTB_CPU | SRSTB_CPU;
    u32 val_reset = val & ~(RSTB_CPU | SRSTB_CPU);

    // wait -> 2cycle reset seems to cause it to fail with reset_attempt=0x30

    write32(LT_RESETS_COMPAT, val);
    for(int i = 0; i < width; i++)
    {
        __asm volatile ("\n");
    }
    write32(LT_RESETS_COMPAT, val_reset);
    for(int i = 0; i < 2; i++)
    {
        __asm volatile ("\n");
    }
    //udelay(100);
    write32(LT_RESETS_COMPAT, val);
}

// Injects an HRESET glitch pulse.
void ppc_hreset_glitch(int width)
{
    u32 val = read32(LT_RESETS_COMPAT) | RSTB_CPU;
    u32 val_reset = val & ~RSTB_CPU;

    write32(LT_RESETS_COMPAT, val_reset);
    for(int i = 0; i < width; i++)
    {
        __asm volatile ("\n");
    }
    write32(LT_RESETS_COMPAT, val);
}

// Races the PPC
void* ppc_race(void)
{
    ppc_hold_resets();

    u32* body = (void*) ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
    if(!body) {
        printf("PPC: failed to load signed image.\n");
        return NULL;
    }

    u32* boot_state = (u32*) 0x016FFFE0;
    memset(boot_state, 0, 0x20);
    dc_flushrange(boot_state, 0x20);

    u32 old = *body;
    ppc_reset();

    u8 exit_code = 0x00;
    do {
        dc_invalidaterange(body, sizeof(u32));
        dc_invalidaterange(boot_state, 0x20);
        exit_code = boot_state[7] >> 24;
    } while(old == *body && exit_code == 0x00);

    if(exit_code != 0x00 && old == *body) {
        printf("PPC: ROM failure: 0x%08lX.\n", boot_state[7]);
        return NULL;
    }

    return body;
}

// Jump stub to place at exception vectors and such.
void ppc_jump_stub(u32 location, u32 entry)
{
    size_t i = 0;

    // lis r3, entry@h
    write32(location + i, 0x3C600000 | entry >> 16); i += sizeof(u32);
    // ori r3, r3, entry@l
    write32(location + i, 0x60630000 | (entry & 0xFFFF)); i += sizeof(u32);
    // mtsrr0 r3
    write32(location + i, 0x7C7A03A6); i += sizeof(u32);
    // li r3, 0
    write32(location + i, 0x38600000); i += sizeof(u32);
    // mtsrr1 r3
    write32(location + i, 0x7C7B03A6); i += sizeof(u32);
    // rfi
    write32(location + i, 0x4C000064); i += sizeof(u32);

    dc_flushrange((void*)location, i);
}

// Wait stub which accepts void** addr containing a ptr to jump to, when it is written non-0
void ppc_wait_stub(u32 location, u32 entry_ptr)
{
    size_t i = 0;
    
    // lis r3, entry_ptr@h
    write32(location + i, 0x3C600000 | entry_ptr >> 16); i += sizeof(u32);
    // ori r3, r3, entry_ptr@l
    write32(location + i, 0x60630000 | (entry_ptr & 0xFFFF)); i += sizeof(u32);

    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // stw r4, 0(r3)
    write32(location + i, 0x90830000); i += sizeof(u32);
    
    // clrrwi r5, r3, 5
    write32(location + i, 0x54650034); i += sizeof(u32);
    // dcbf r0, r5
    write32(location + i, 0x7C0028AC); i += sizeof(u32);
    // isync
    write32(location + i, 0x4C00012C); i += sizeof(u32);
    // sync
    write32(location + i, 0x7C0004AC); i += sizeof(u32);

// _wait:
    // dcbi r0, r3
    write32(location + i, 0x7C001BAC); i += sizeof(u32);
    // sync
    write32(location + i, 0x7C0004AC); i += sizeof(u32);
    // lwz r4, 0(r3)
    write32(location + i, 0x80830000); i += sizeof(u32);
    // cmpwi cr7, r4, 0
    write32(location + i, 0x2F840000); i += sizeof(u32);
    // beq cr7, _wait
    write32(location + i, 0x419EFFF0); i += sizeof(u32);

    // mtsrr0 r4
    write32(location + i, 0x7C9A03A6); i += sizeof(u32);
    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // mtsrr1 r4
    write32(location + i, 0x7C9B03A6); i += sizeof(u32);
    // rfi
    write32(location + i, 0x4C000064); i += sizeof(u32);

    dc_flushrange((void*)location, i);
}

// OTP/bootrom dumping stub
void ppc_dump_stub(u32 location, u32 dump_block_addr)
{
    size_t i = 0;

    // mfl2cr r3
    write32(location + i, 0x7c79faa6); i += sizeof(u32);
    // lis r4, 0x7FFF
    write32(location + i, 0x3c807fff); i += sizeof(u32);
    // ori r4, r4, 0xFFFF
    write32(location + i, 0x6084ffff); i += sizeof(u32);
    // and r3, r3, r4
    write32(location + i, 0x7c632038); i += sizeof(u32);
    // mtl2cr r3
    write32(location + i, 0x7c79fba6); i += sizeof(u32);
    // sync
    write32(location + i, 0x7c0004ac); i += sizeof(u32);
    // mfdbsr r3
    write32(location + i, 0x7c70faa6); i += sizeof(u32);
    // lis r4, 0xFFFF
    write32(location + i, 0x3c80ffff); i += sizeof(u32);
    // ori r4, r4, 0x3FFF
    write32(location + i, 0x60843fff); i += sizeof(u32);
    // and r3, r3, r4
    write32(location + i, 0x7c632038); i += sizeof(u32);
    // mtdbsr r3
    write32(location + i, 0x7c70fba6); i += sizeof(u32);
    // sync
    write32(location + i, 0x7c0004ac); i += sizeof(u32);
    

    // lis r3, dump_block_addr@h
    write32(location + i, 0x3C600000 | dump_block_addr >> 16); i += sizeof(u32);
    // ori r3, r3, dump_block_addr@l
    write32(location + i, 0x60630000 | (dump_block_addr & 0xFFFF)); i += sizeof(u32);

    //
    // Dump interesting SPRs
    //

    // mfspr r4, 0x3b3
    write32(location + i, 0x7c93eaa6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x0)); i += sizeof(u32);

    // mfspr r4, hid0
    write32(location + i, 0x7c90faa6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x4)); i += sizeof(u32);

    // mfspr r4, hid1
    write32(location + i, 0x7c91faa6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x8)); i += sizeof(u32);

    // mfspr r4, hid2
    write32(location + i, 0x7c98e2a6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0xc)); i += sizeof(u32);

    // mfspr r4, hid3 (TODO? li r4, 0)
    write32(location + i, 0x38800000); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x10)); i += sizeof(u32);

    // mfspr r4, hid4
    write32(location + i, 0x7c93faa6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x14)); i += sizeof(u32);

    // mfspr r4, hid5
    write32(location + i, 0x7c90eaa6); i += sizeof(u32);
    write32(location + i, 0x90830000 | (DUMP_BLOCK_SPR_OFFS+0x18)); i += sizeof(u32);

    // move bootrom test u32 to 0x0...
    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // stw r4, 0x10(r3)
    write32(location + i, 0x90830010); i += sizeof(u32);

    //
    // Dump eFuses
    //

    // lis r3, dump_block_addr@h
    write32(location + i, 0x3C600000 | (dump_block_addr+DUMP_BLOCK_EFUSE_OFFS) >> 16); i += sizeof(u32);
    // ori r3, r3, dump_block_addr@l
    write32(location + i, 0x60630000 | ((dump_block_addr+DUMP_BLOCK_EFUSE_OFFS) & 0xFFFF)); i += sizeof(u32);
    // lis r4, efuse_to_read@h
    write32(location + i, 0x3C800000 | ESPRESSO_EFUSE_ADDR >> 16); i += sizeof(u32);
    // ori r4, r4, efuse_to_read@l
    write32(location + i, 0x60840000 | (ESPRESSO_EFUSE_ADDR & 0xFFFF)); i += sizeof(u32);


    write32(location + i, 0x3ca00000); i += sizeof(u32);// lis r5, 0
    write32(location + i, 0x3cc00000); i += sizeof(u32);// lis r6, 0
    write32(location + i, 0x2c060000 | EFUSE_BLOCK_LEN); i += sizeof(u32); // cmpwi r6, 0x40
    write32(location + i, 0x4080001c); i += sizeof(u32); // bge- 0x4060
    write32(location + i, 0x80a40000); i += sizeof(u32); // lwz r5, 0(r4)
    write32(location + i, 0x90a30000); i += sizeof(u32); // stw r5, 0(r3)
    write32(location + i, 0x38630004); i += sizeof(u32); // addi r3, r3, 4
    write32(location + i, 0x38840004); i += sizeof(u32); // addi r4, r4, 4
    write32(location + i, 0x38c60004); i += sizeof(u32); // addi r6, r6, 4
    write32(location + i, 0x4bffffe4); i += sizeof(u32); // b 0x4040

    //
    // Dump bootrom
    //

    // lis r3, dump_block_addr@h
    write32(location + i, 0x3C600000 | (dump_block_addr+DUMP_BLOCK_BOOTROM_OFFS) >> 16); i += sizeof(u32);
    // ori r3, r3, dump_block_addr@l
    write32(location + i, 0x60630000 | ((dump_block_addr+DUMP_BLOCK_BOOTROM_OFFS) & 0xFFFF)); i += sizeof(u32);
    // lis r4, efuse_to_read@h
    write32(location + i, 0x3C800000 | ESPRESSO_BOOTROM_ADDR >> 16); i += sizeof(u32);
    // ori r4, r4, efuse_to_read@l
    write32(location + i, 0x60840000 | (ESPRESSO_BOOTROM_ADDR & 0xFFFF)); i += sizeof(u32);


    write32(location + i, 0x3ca00000); i += sizeof(u32);// lis r5, 0
    write32(location + i, 0x3cc00000); i += sizeof(u32);// lis r6, 0
    write32(location + i, 0x2c060000 | BOOTROM_LEN); i += sizeof(u32); // cmpwi r6, 0x40
    write32(location + i, 0x4080001c); i += sizeof(u32); // bge- 0x4060
    write32(location + i, 0x80a40000); i += sizeof(u32); // lwz r5, 0(r4)
    write32(location + i, 0x90a30000); i += sizeof(u32); // stw r5, 0(r3)
    write32(location + i, 0x38630004); i += sizeof(u32); // addi r3, r3, 4
    write32(location + i, 0x38840004); i += sizeof(u32); // addi r4, r4, 4
    write32(location + i, 0x38c60004); i += sizeof(u32); // addi r6, r6, 4
    write32(location + i, 0x4bffffe4); i += sizeof(u32); // b 0x4040

    //
    // Read the bootrom test u32 last to verify everything made it in fine
    //

    // lis r3, dump_block_addr@h
    write32(location + i, 0x3C600000 | dump_block_addr >> 16); i += sizeof(u32);
    // ori r3, r3, dump_block_addr@l
    write32(location + i, 0x60630000 | (dump_block_addr & 0xFFFF)); i += sizeof(u32);

    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // lwz r4, 0(r4)
    write32(location + i, 0x80840000); i += sizeof(u32);
    // stw r4, 0x10(r3)
    write32(location + i, 0x90830010); i += sizeof(u32);

    // Final u32 write to dump_block_addr+0, signaling completion.
    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // stw r4, 0(r3)
    write32(location + i, 0x90830000); i += sizeof(u32);

    // b .
    //write32(location + i, 0x48000000); i += sizeof(u32);

    // Flush caches
    write32(location + i, 0x3cc00000); i += sizeof(u32);// lis r6, 0
    write32(location + i, 0x2c060000 | DUMP_BLOCK_END); i += sizeof(u32); // cmpwi r6, 0x40
    write32(location + i, 0x40800020); i += sizeof(u32); // bge- _end
    write32(location + i, 0x54650034); i += sizeof(u32); // clrrwi r5, r3, 5
    write32(location + i, 0x7c0028ac); i += sizeof(u32); // dcbf r0, r5
    write32(location + i, 0x4c00012c); i += sizeof(u32); // isync
    write32(location + i, 0x7c0004ac); i += sizeof(u32); // sync
    write32(location + i, 0x38630020); i += sizeof(u32); // addi r3, r3, 0x20
    write32(location + i, 0x38c60020); i += sizeof(u32); // addi r6, r6, 0x20
    write32(location + i, 0x4bffffe0); i += sizeof(u32); // b _loop

    // TODO: somehow this busy loop still triggers a hardware watchdog somewhere

    // busy: nop
    write32(location + i, 0x60000000); i += sizeof(u32);
    // mov r3 0
    write32(location + i, 0x38600000); i += sizeof(u32);
    // nop
    write32(location + i, 0x60000000); i += sizeof(u32);
    // b busy
    write32(location + i, 0x4BFFFFF4); i += sizeof(u32);

    dc_flushrange((void*)location, i);
}

static bool ready = false;

void ppc_prepare(void)
{
    if(ready) {
        printf("PPC: PowerPC is already prepared!\n");
        return;
    }

    // Boot the PowerPC and race the ROM.
    u32 start = (u32) ppc_race();
    if(start == 0) return;

    // The wait stub clears this to zero before entering the wait loop.
    // Set a different value first, so we know when it enters the loop.
    write32(0x00000000, 0xFFFFFFFF);
    dc_flushrange((void*)0x00000000, sizeof(u32));

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_wait_stub(0x00000100, 0x00000000);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    ppc_jump_stub(start, 0x00000100);

    // Wait for the PowerPC to enter the wait loop.
    while(true) {
        dc_invalidaterange((void*)0x00000000, sizeof(u32));
        if(read32(0x00000000) == 0) break;
    }

    printf("PPC: PowerPC is waiting for entry!\n");
    ready = true;
}

void ppc_jump(u32 entry)
{
    if(!ready) {
        printf("PPC: Jump requested but PowerPC not ready!\n");
        return;
    }

    // Write the PowerPC entry point.
    write32(0x00000000, entry);
    dc_flushrange((void*)0x00000000, sizeof(u32));

    ready = false;
}

int _ppc_dump_bootrom_otp(int delay)
{
    u32 cookie, old;
    u32 safety_exit = 0;
    u8 exit_code = 0x00;

    uintptr_t dump_block = DUMP_BLOCK_ADDR;
    uintptr_t dump_stub_addr = DUMP_STUB_ADDR;

    printf("PPC: Dumping OTP and bootrom.\n");

    // Hold the PPC in reset before we write to RAM which the PPC can also write to.
    ppc_hold_resets();

    // TODO: I'm not sure if these are required
    set32(LT_COMPAT, LT_COMPAT_BOOT_CODE);
    set32(LT_AHBPROT, 0xFFFFFFFF);

    ppc_prepare_compat_and_straps(0);
    ppc_extra_safe_boot_pumping_for_bruteforcing();

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_dump_stub(dump_stub_addr, dump_block);
    ppc_jump_stub(0x100, dump_stub_addr);
    write32(0x100, 0x48003f00); // b 0x4000
    dc_flushrange((void*)0x100, sizeof(u32));

    // Make it easier to tell if we fail bootrom reads
    write32(0, 0xFFFFFFFF);
    dc_flushrange((void*)0, sizeof(u32));

    // Clear out the dump_block block
    // The wait stub clears dump_block+0 to zero before entering the wait loop.
    // Set a different value first, so we know when it enters the loop.
    for (int i = 0; i < DUMP_BLOCK_END; i += 4)
    {
        write32(dump_block+i, 0xFFFFFFFF);
        
    }
    dc_flushrange((void*)dump_block, DUMP_BLOCK_END);

    // Clear the PPC's boot state
    u32* boot_state = (u32*) 0x016FFFE0;
    memset(boot_state, 0xFF, 0x20);
    dc_flushrange(boot_state, 0x20);

    // leftovers, TODO remove
    u32* body = (u32*)0x08000100;
    old = *body;

    // Kill IRQs so that all of our timing is precise.
    cookie = irq_kill();

    ppc_release_reset();
    //ppc_do_defuse(spacing);

    for(int i = 0; i < delay; i++)
    {
        __asm volatile ("\n");
    }

    // Check reset vector
    dc_invalidaterange((void*)0x100, sizeof(u32));
    u32 tmp = read32(0x100);

    //ppc_hreset_glitch(spacing);
    ppc_do_sreset();

    // Wait for the PowerPC to enter the wait loop.
    safety_exit = 0;
    while(true) {
        dc_invalidaterange((void*)dump_block, sizeof(u32));
        if(read32(dump_block) == 0) break;

        udelay(1000);

        safety_exit++;
        if (safety_exit > 500) {
            break;
        }
    }

    // OK we're done, restore IRQs
    irq_restore(cookie);

    // Read everything out
    dc_invalidaterange((void*)dump_block, DUMP_BLOCK_END);
    dc_invalidaterange(boot_state, 0x20);
    dc_invalidaterange(body, sizeof(u32));
    dc_invalidaterange((void*)0x100, sizeof(u32));

    u32* spr_arr = (u32*)(dump_block + DUMP_BLOCK_SPR_OFFS);
    u32* fuse_arr = (u32*)(dump_block + DUMP_BLOCK_EFUSE_OFFS);
    u32* bootrom_arr = (u32*)(dump_block + DUMP_BLOCK_BOOTROM_OFFS);

    printf("PPC: i=%08lx timer=%08lX err=0x%08lX ancast=0x%08lX->0x%08lX bootrom+0=%08lX,%08lX vec=%08lX.\n", safety_exit, boot_state[4], boot_state[7], old, *body, *(u32*)(dump_block+0x00000010), bootrom_arr[0], *(u32*)0x100);
    printf("PPC:   scr=%08lX\n", spr_arr[0]);
    printf("PPC:   hid0=%08lX   hid1=%08lX   hid2=%08lX   hid3=%08lX\n", spr_arr[1], spr_arr[2], spr_arr[3], spr_arr[4]);
    printf("PPC:   hid4=%08lX   hid5=%08lX\n", spr_arr[5], spr_arr[6]);
    printf("PPC:  fuse0=%08lX  fuse1=%08lX  fuse2=%08lX  fuse3=%08lX\n", fuse_arr[0], fuse_arr[1], fuse_arr[2], fuse_arr[3]);
    printf("PPC:  fuse4=%08lX  fuse5=%08lX  fuse6=%08lX  fuse7=%08lX\n", fuse_arr[4], fuse_arr[5], fuse_arr[6], fuse_arr[7]);
    printf("PPC:  fuse8=%08lX  fuse9=%08lX fuse10=%08lX fuse11=%08lX\n", fuse_arr[8], fuse_arr[9], fuse_arr[10], fuse_arr[11]);
    printf("PPC: fuse12=%08lX fuse13=%08lX fuse14=%08lX fuse15=%08lX\n\n", fuse_arr[12], fuse_arr[13], fuse_arr[14], fuse_arr[15]);

    // Check if we were actually successful
    u32 fuse_val = fuse_arr[0];
    u32 bootrom_val = bootrom_arr[0];
    if (fuse_val != 0xFFFFFFFF && fuse_val != 0x0 && fuse_val != 0x0BADBABE) {
        goto dump;
    }
    if (bootrom_val != 0xFFFFFFFF && bootrom_val != 0x0 && bootrom_val != 0x0BADBABE) {
        goto dump;
    }

    return 0;

dump:
    printf("Got a successful SRESET!\n");

    // Dump to files
    printf("Dumping Espresso bootrom to `sdmc:/espresso_bootrom.bin`... ");
    FILE* f_bootrom = fopen("sdmc:/espresso_bootrom.bin", "wb");
    if(!f_bootrom)
    {
        printf("\nFailed to open sdmc:/espresso_bootrom.bin.\n");
    }
    else {
        fwrite(bootrom_arr, 1, BOOTROM_LEN, f_bootrom);
        fclose(f_bootrom);
        printf("Done.\n");
    }

    printf("Dumping Espresso OTP to `sdmc:/espresso_otp.bin`... ");
    FILE* f_otp = fopen("sdmc:/espresso_otp.bin", "wb");
    if(!f_otp)
    {
        printf("\nFailed to open sdmc:/espresso_otp.bin.\n");
    }
    else {
        fwrite(fuse_arr, 1, EFUSE_BLOCK_LEN, f_otp);
        fclose(f_otp);
        printf("Done.\n");
    }

    return 1;
}

void ppc_test(u32 val)
{
    for (int i = 0; i < 0x1000; i++)
    {
        printf("%08x %08x %08x\n", exi0_read32(0x21000400), rtc_get_ctrl0(), rtc_get_ctrl1());
    }
    //ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
    //memcpy((void*)0x01330000, (void*)0x08000000, 0x11e100);

    for (int i = 0x22fd0; i < 0x22fd0+0x30; i += 0x1)
    {
        printf("PPC: wait delay: 0x%02x\n", i);
        if (_ppc_dump_bootrom_otp(i)) break;
    }
}

void ppc_dump_bootrom_otp(void)
{
    // Notes on timing (just SRESET, no watcher)
    // 0x216e0+?? loops - bss wiped
    // 0x22fe0 loops - winner
    // 0x5b9c00 loops - limbo
    // 0x5bd6c6 loops - bootrom-only winner
    // 0x5bff00 loops - decrypted, verified, timer written

    for (int i = 0x22fd0; i < 0x22fd0+0x30; i += 0x1)
    {
        printf("PPC: wait delay: 0x%02x\n", i);
        if (_ppc_dump_bootrom_otp(i)) break;
    }
}