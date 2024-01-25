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

#include <string.h>

#if 0
void _ppc_test_goofy(u32 val, int spacing)
{
    u32 cookie;
    u32 safety_exit = 0;

    // Boot the PowerPC and race the ROM.
    u32 start = 0;
    uintptr_t entry = 0x08200000;
    uintptr_t wait_stub_addr = 0x08000200;
    
    // The wait stub clears this to zero before entering the wait loop.
    // Set a different value first, so we know when it enters the loop.
    write32(entry, 0xFFFFFFFF);
    dc_flushrange((void*)entry, sizeof(u32));

    write32(entry+0x00000004, 0xFFFFFFFF);
    write32(entry+0x00000008, 0xFFFFFFFF);
    write32(entry+0x0000000C, 0xFFFFFFFF);
    dc_flushrange((void*)(entry+0x00000004), sizeof(u32)*3);

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    ppc_jump_stub(start, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);

    //{
        ppc_hold_resets();

        ppc_prepare_config(val);
        ppc_prime_defuse();

        /*u32* body = (void*) ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
        if(!body) {
            printf("PPC: failed to load signed image.\n");
            return NULL;
        }*/
        //u32* body = (u32*)0x01330000;
        u32* body = (u32*)0x08000100;
        memcpy((void*)0x08000000, (void*)0x01330000, 0x11e100);
        dc_flushrange((void*)0x08000000, 0x11e100);
        
        //memset(0x01330000, 0, 0x200);
        //memset(0x08000000, 0, 0x100);
        //memset(0x01330000, 0, 0x100);
        //dc_flushrange(0x08000000, 0x100);
        //dc_flushrange(0x01330000, 0x100);

        //ppc_wait_stub(body, 0x00000000);
        //dc_flushrange(body, 0x200);

        u32* rom_state = (u32*) 0x016FFFE0;
        memset(rom_state, 0, 0x20);
        dc_flushrange(rom_state, 0x20);

        //printf("Doing reset...\n");

        cookie = irq_kill();

        u32 old = *body;
        ppc_release_reset();
        //ppc_do_defuse(spacing);
        //ppc_release_reset();

#if 1
        safety_exit = 0;
        u8 exit_code = 0x00;
        do 
        {
            dc_invalidaterange(body, sizeof(u32));
            dc_invalidaterange(rom_state, 0x20);
            exit_code = rom_state[7] >> 24;
            safety_exit += 1;
            if (safety_exit > 0x10000) {
                break;
            }
        } while(old == *body && exit_code == 0x00);

        if(exit_code != 0x00 || old == *body) {
            irq_restore(cookie);

            dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*3);
            dc_invalidaterange(rom_state, 0x20);
            printf("PPC: ROM failure: timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX.\n", rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C));
            return;
        }
#endif
        //

        start = body;
    //}

    if(start == 0) {
        irq_restore(cookie);
        printf("Failed to race PPC, abort\n");
        return;
    }

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    ppc_jump_stub(start, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);

    //ppc_do_glitch(spacing);
    /*for(int i = 0; i < spacing; i++)
    {
        __asm volatile ("\n");
    }*/
    udelay(spacing);
    clear32(LT_RESETS_COMPAT, SRSTB_CPU);

    memcpy((void*)0x08000000, (void*)0x01330000, 0x11e100);
    dc_flushrange((void*)0x08000000, 0x11e100);

    udelay(100);
    set32(LT_RESETS_COMPAT, SRSTB_CPU);

#if 1
        safety_exit = 0;
        exit_code = 0x00;
        do 
        {
            dc_invalidaterange(body, sizeof(u32));
            dc_invalidaterange(rom_state, 0x20);
            exit_code = rom_state[7] >> 24;
            safety_exit += 1;
            if (safety_exit > 0x10000) {
                break;
            }
        } while(old == *body && exit_code == 0x00);

        if(exit_code != 0x00 || old == *body) {
            irq_restore(cookie);

            dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*3);
            dc_invalidaterange(rom_state, 0x20);
            printf("PPC: ROM failure: timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX.\n", rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C));
            return;
        }
#endif
        //

        start = body;
    //}

    if(start == 0) {
        irq_restore(cookie);
        printf("Failed to race PPC, abort\n");
        return;
    }

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    ppc_jump_stub(start, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);

    // Wait for the PowerPC to enter the wait loop.
    safety_exit = 0;
    while(true) {
        dc_invalidaterange((void*)entry, sizeof(u32));
        if(read32(entry) == 0) break;

        safety_exit++;
        if (safety_exit > 0x100000) {
            break;
        }
    }
    irq_restore(cookie);

    dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*3);
    dc_invalidaterange(rom_state, 0x20);

    printf("PPC: ROM success? %08x timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX.\n", safety_exit, rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C));

    //printf("PPC: PowerPC is waiting for entry!\n");
}

int _ppc_test(u32 val, int spacing)
{
    u32 cookie;
    u32 safety_exit = 0;
    u8 exit_code = 0x00;

    // Boot the PowerPC and race the ROM.
    u32 start = 0;
    uintptr_t entry = 0x08200000;
    uintptr_t wait_stub_addr = 0x4000;
    
    // The wait stub clears this to zero before entering the wait loop.
    // Set a different value first, so we know when it enters the loop.
    write32(entry, 0xFFFFFFFF);
    dc_flushrange((void*)entry, sizeof(u32));
    write32(0, 0xFFFFFFFF);
    dc_flushrange((void*)0, sizeof(u32));

    write32(entry+0x00000004, 0xFFFFFFFF);
    write32(entry+0x00000008, 0xFFFFFFFF);
    write32(entry+0x0000000C, 0xFFFFFFFF);
    write32(entry+0x00000010, 0xFFFFFFFF);
    dc_flushrange((void*)(entry+0x00000004), sizeof(u32)*4);

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    ppc_jump_stub(0x08000100, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);

    //{
        ppc_hold_resets();

        set32(LT_COMPAT, LT_COMPAT_BOOT_CODE);
        set32(LT_AHBPROT, 0xFFFFFFFF);

        ppc_prepare_config(val);
        ppc_prime_defuse();

        /*u32* body = (void*) ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
        if(!body) {
            printf("PPC: failed to load signed image.\n");
            return NULL;
        }*/
        //u32* body = (u32*)0x01330000;
        u32* body = (u32*)0x08000100;
        memcpy((void*)0x08000000, (void*)0x01330000, 0x11e100);
        dc_flushrange((void*)0x08000000, 0x11e100);
        
        //memset(0x01330000, 0, 0x200);
        //memset(0x08000000, 0, 0x100);
        //memset(0x01330000, 0, 0x100);
        //dc_flushrange(0x08000000, 0x100);
        //dc_flushrange(0x01330000, 0x100);

        //ppc_wait_stub(body, 0x00000000);
        //dc_flushrange(body, 0x200);

        u32* rom_state = (u32*) 0x016FFFE0;
        memset(rom_state, 0xFF, 0x20);
        dc_flushrange(rom_state, 0x20);

        //printf("Doing reset...\n");

        cookie = irq_kill();

        u32 old = *body;
        ppc_release_reset();
        //ppc_do_defuse(spacing);
        //ppc_release_reset();

#if 1
        safety_exit = 0;
        exit_code = 0x00;
        do 
        {
            dc_invalidaterange(body, sizeof(u32));
            dc_invalidaterange(rom_state, 0x20);
            exit_code = rom_state[7] >> 24;
            safety_exit += 1;
            if (safety_exit > 0x10000) {
                break;
            }
        } while(old == *body && (exit_code == 0xFF || exit_code == 0x00));

        if((exit_code != 0x00 && exit_code != 0xFF) || old == *body) {
            irq_restore(cookie);

            dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*4);
            dc_invalidaterange(rom_state, 0x20);
            printf("PPC: ROM failure: timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX, test=%08lX.\n", rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C), *(u32*)(entry+0x00000010));
            return 0;
        }
#endif
        //

        start = body;
    //}

#if 1
    if(start == 0) {
        irq_restore(cookie);
        printf("Failed to race PPC, abort\n");
        return 0;
    }

    // Copy the wait stub to MEM1. This waits for us to load further code.
    //ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    //ppc_jump_stub(start, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);
#endif

    //ppc_do_glitch(spacing);
    for(int i = 0; i < spacing; i++)
    {
        __asm volatile ("\n");
    }
    //udelay(spacing);
    ppc_do_sreset();
    udelay(1000);

    // Wait for the PowerPC to enter the wait loop.
    safety_exit = 0;
    while(true) {
        dc_invalidaterange((void*)entry, sizeof(u32));
        if(read32(entry) == 0) break;

        dc_invalidaterange(rom_state, 0x20);
        exit_code = rom_state[7] >> 24;
        if (exit_code != 0 && exit_code != 0x80) break;

        safety_exit++;
        if (safety_exit > 0x10000) {
            break;
        }
    }
    irq_restore(cookie);

    dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*4);
    dc_invalidaterange(rom_state, 0x20);

    printf("PPC: ROM success? %08x timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX, test=%08lX.\n", safety_exit, rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C), *(u32*)(entry+0x00000010));

    u32 fuse_val = *(u32*)(entry+0x00000008);
    if (fuse_val != 0xFFFFFFFF && fuse_val != 0x0BADBABE) {
        return 1;
    }

    //printf("PPC: PowerPC is waiting for entry!\n");
    return 0;
}

void ppc_test(u32 val)
{
    ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
    memcpy((void*)0x01330000, (void*)0x08000000, 0x11e100);

    //for (int i = 0x170680; i < 0x170700; i += 0x1)

    // 0x8CDus - bss wiped
    // 0x8CDus - flags/OTP are parsed
    // 0x1700us - almost copying, verifying
    // 0x12700us - start copying
    // 0x18700us - done verifying

    // 0x216d0 loops - bss wiped

    // with watcher
    // 0x1716D0 done decrypting weird limbo
    // 0x1742d0 wtf, bootrom dumpable, spr=0xc0000000
    // 0x1743d0 end limbo, execute payload w/ SRESET, spr=0x80600000


    for (int i = 0x1741d0; i < 0x1741d0+0x1000; i += 0x10)
    {
        printf("iteration: %02x\n", i);
        if (_ppc_test(val, i)) break;
    }
    
}

#endif

void ppc_hold_resets(void)
{
    clear32(LT_RESETS_COMPAT, RSTB_PI | RSTB_CPU | SRSTB_CPU);
    udelay(100);
}

void ppc_reset(void)
{
    ppc_hold_resets();

    mask32(LT_COMPAT_MEMCTRL_STATE, 0xFFE0022F, 0x08100000);

    clear32(LT_60XE_CFG, 8);
    mask32(LT_60XE_CFG, 0x1C0000, 0x20000);
    set32(LT_60XE_CFG, 0xC000);

    set32(LT_RESETS_COMPAT, RSTB_PI);
    clear32(LT_COMPAT_MEMCTRL_STATE, 0x20); // PPC clock mult

    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
}

void ppc_prepare_config(u32 val)
{
    ppc_hold_resets();

    //mask32(LT_COMPAT_MEMCTRL_STATE, 0x3FF, 0x26C);
    //set32(LT_COMPAT_MEMCTRL_STATE, 0x200);

    mask32(LT_COMPAT_MEMCTRL_STATE, 0xFFE0022F, 0x08100000 | val);

    clear32(LT_60XE_CFG, 8);
    mask32(LT_60XE_CFG, 0x1C0000, 0x20000);
    set32(LT_60XE_CFG, 0xC000);

    set32(LT_RESETS_COMPAT, RSTB_PI);
    clear32(LT_COMPAT_MEMCTRL_STATE, 0x20); // PPC clock mult
    //set32(LT_COMPAT_MEMCTRL_STATE, 0x20);
}

void ppc_prime_defuse()
{
    //printf("Release reset\n");

    // Read OTP fully at least once, then hold
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
    clear32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);

    // Read it again just in case
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
    clear32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
    udelay(20000);
}

void ppc_release_reset()
{
    set32(LT_RESETS_COMPAT, RSTB_CPU | SRSTB_CPU);
}

void ppc_do_defuse(int reset_attempt)
{
    //printf("de_Fuse?\n");

#if 1
    // Timing sensitive part
    set32(LT_RESETS_COMPAT, RSTB_CPU);
    for(int i = 0; i < reset_attempt; i++)
    {
        __asm volatile ("\n");
    }
    clear32(LT_RESETS_COMPAT, RSTB_CPU);
    udelay(100);
#endif

    // Boot normally now
    set32(LT_RESETS_COMPAT, RSTB_CPU);
}

void ppc_do_sreset()
{
    clear32(LT_RESETS_COMPAT, SRSTB_CPU);
    udelay(100);
    set32(LT_RESETS_COMPAT, SRSTB_CPU);
}

void ppc_do_glitch(int reset_attempt)
{
    // Timing sensitive part
    u32 val = read32(LT_RESETS_COMPAT) | RSTB_CPU;
    u32 val_reset = val & ~RSTB_CPU;
    /*clear32(LT_RESETS_COMPAT, RSTB_CPU);
    for(int i = 0; i < reset_attempt; i++)
    {
        __asm volatile ("\n");
    }
    set32(LT_RESETS_COMPAT, RSTB_CPU);*/
    //write32(LT_RESETS_COMPAT, val_reset);
    //write32(LT_RESETS_COMPAT, val);
}

void* ppc_race(void)
{
    ppc_hold_resets();

    u32* body = (void*) ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
    if(!body) {
        printf("PPC: failed to load signed image.\n");
        return NULL;
    }

    u32* rom_state = (u32*) 0x016FFFE0;
    memset(rom_state, 0, 0x20);
    dc_flushrange(rom_state, 0x20);

    u32 old = *body;
    ppc_reset();

    u8 exit_code = 0x00;
    do {
        dc_invalidaterange(body, sizeof(u32));
        dc_invalidaterange(rom_state, 0x20);
        exit_code = rom_state[7] >> 24;
    } while(old == *body && exit_code == 0x00);

    if(exit_code != 0x00 && old == *body) {
        printf("PPC: ROM failure: 0x%08lX.\n", rom_state[7]);
        return NULL;
    }

    return body;
}

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

void ppc_wait_stub(u32 location, u32 entry)
{
    size_t i = 0;
    
    // lis r3, entry@h
    write32(location + i, 0x3C600000 | entry >> 16); i += sizeof(u32);
    // ori r3, r3, entry@l
    write32(location + i, 0x60630000 | (entry & 0xFFFF)); i += sizeof(u32);


    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // stw r4, 0(r3)
    write32(location + i, 0x90830000); i += sizeof(u32);
    

    // dcbf r0, r3
    write32(location + i, 0x7C0018AC); i += sizeof(u32);
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

void ppc_dump_stub(u32 location, u32 entry)
{
    size_t i = 0;

    u32 efuse_to_read = 0x0C32003C;

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

    // lis r3, efuse_to_read@h
    write32(location + i, 0x3C600000 | efuse_to_read >> 16); i += sizeof(u32);
    // ori r3, r3, efuse_to_read@l
    write32(location + i, 0x60630000 | (efuse_to_read & 0xFFFF)); i += sizeof(u32);
    // lwz r4, 0(r3)
    write32(location + i, 0x80830000); i += sizeof(u32);
    

    // lis r3, entry@h
    write32(location + i, 0x3C600000 | entry >> 16); i += sizeof(u32);
    // ori r3, r3, entry@l
    write32(location + i, 0x60630000 | (entry & 0xFFFF)); i += sizeof(u32);

    // stw r4, 8(r3)
    write32(location + i, 0x90830008); i += sizeof(u32);

    // mfspr r4, 0x3b3
    write32(location + i, 0x7C93EAA6); i += sizeof(u32);
    // stw r4, 4(r3)
    write32(location + i, 0x90830004); i += sizeof(u32);

    // li r4, 0xf00f
    write32(location + i, 0x3880F00F); i += sizeof(u32);
    // stw r4, 0xC(r3)
    write32(location + i, 0x9083000C); i += sizeof(u32);

    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // lwz r4, 0(r4)
    write32(location + i, 0x80840000); i += sizeof(u32);
    // stw r4, 0x10(r3)
    write32(location + i, 0x90830010); i += sizeof(u32);


    // li r4, 0
    write32(location + i, 0x38800000); i += sizeof(u32);
    // stw r4, 0(r3)
    write32(location + i, 0x90830000); i += sizeof(u32);

    // b .
    //write32(location + i, 0x48000000); i += sizeof(u32);

    // dcbf r0, r3
    write32(location + i, 0x7C0018AC); i += sizeof(u32);
    // sync
    write32(location + i, 0x7C0004AC); i += sizeof(u32);

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

int _ppc_test(u32 val, int spacing)
{
    u32 cookie;
    u32 safety_exit = 0;
    u8 exit_code = 0x00;

    // Boot the PowerPC and race the ROM.
    u32 start = 0;
    uintptr_t entry = 0x08200000;
    uintptr_t wait_stub_addr = 0x4000;

    ppc_hold_resets();

    // Copy the wait stub to MEM1. This waits for us to load further code.
    ppc_dump_stub(wait_stub_addr, entry);
    ppc_jump_stub(0x100, wait_stub_addr);
    write32(0x100, 0x48003f00); // b 0x4000
    dc_flushrange(0x100, sizeof(u32));

    // The wait stub clears this to zero before entering the wait loop.
    // Set a different value first, so we know when it enters the loop.
    write32(entry, 0xFFFFFFFF);
    dc_flushrange((void*)entry, sizeof(u32));
    write32(0, 0xFFFFFFFF);
    dc_flushrange((void*)0, sizeof(u32));

    write32(entry+0x00000004, 0xFFFFFFFF);
    write32(entry+0x00000008, 0xFFFFFFFF);
    write32(entry+0x0000000C, 0xFFFFFFFF);
    write32(entry+0x00000010, 0xFFFFFFFF);
    dc_flushrange((void*)(entry+0x00000004), sizeof(u32)*4);

    u32* rom_state = (u32*) 0x016FFFE0;
    memset(rom_state, 0xFF, 0x20);
    dc_flushrange(rom_state, 0x20);

    //{
        

        set32(LT_COMPAT, LT_COMPAT_BOOT_CODE);
        set32(LT_AHBPROT, 0xFFFFFFFF);

        ppc_prepare_config(val);
        ppc_prime_defuse();

        u32* body = (u32*)0x08000100;
        //memcpy((void*)0x08000000, (void*)0x01330000, 0x11e100);
        //dc_flushrange((void*)0x08000000, 0x11e100);

        //printf("Doing reset...\n");

        cookie = irq_kill();

        u32 old = *body;
        ppc_release_reset();
        //ppc_do_defuse(spacing);
        //ppc_release_reset();

#if 0
        safety_exit = 0;
        exit_code = 0x00;
        do 
        {
            dc_invalidaterange(body, sizeof(u32));
            dc_invalidaterange(rom_state, 0x20);
            exit_code = rom_state[7] >> 24;
            safety_exit += 1;
            if (safety_exit > 0x10000) {
                break;
            }
        } while(old == *body && (exit_code == 0xFF || exit_code == 0x00));

        if((exit_code != 0x00 && exit_code != 0xFF) || old == *body) {
            irq_restore(cookie);

            dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*4);
            dc_invalidaterange(rom_state, 0x20);
            printf("PPC: ROM failure: timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX, test=%08lX.\n", rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C), *(u32*)(entry+0x00000010));
            return 0;
        }
#endif
        //

        start = body;
    //}

#if 0
    if(start == 0) {
        irq_restore(cookie);
        printf("Failed to race PPC, abort\n");
        return 0;
    }

    // Copy the wait stub to MEM1. This waits for us to load further code.
    //ppc_wait_stub(wait_stub_addr, entry);
    // Copy the jump stub to the start of the ancast body. This jumps to the wait stub.
    //ppc_jump_stub(start, wait_stub_addr);
    ppc_jump_stub(0x100, wait_stub_addr);
#endif

    //ppc_do_glitch(spacing);
    for(int i = 0; i < spacing; i++)
    {
        __asm volatile ("\n");
    }
    //udelay(spacing);
    //ppc_do_sreset();
    dc_invalidaterange(0x100, sizeof(u32));
    u32 tmp = read32(0x100);

    write32(0x100, 0x48003f00); // b 0x4000
    dc_flushrange(0x100, sizeof(u32));
    clear32(LT_RESETS_COMPAT, SRSTB_CPU);
    //ppc_jump_stub(0x100, wait_stub_addr);
    write32(0x100, 0x48003f00); // b 0x4000
    dc_flushrange(0x100, sizeof(u32));

    dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*4);
    dc_invalidaterange(rom_state, 0x20);
    dc_invalidaterange(body, sizeof(u32));
    dc_invalidaterange(0x100, sizeof(u32));

    printf("PPC: checking, timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX, test=%08lX %08lX %08lX.\n", rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C), *(u32*)(entry+0x00000010), *(u32*)0x100, tmp);

    udelay(100);
    set32(LT_RESETS_COMPAT, SRSTB_CPU);

    udelay(1000);

    // Wait for the PowerPC to enter the wait loop.
    safety_exit = 0;
    while(true) {
        dc_invalidaterange((void*)entry, sizeof(u32));
        if(read32(entry) == 0) break;

        dc_invalidaterange(rom_state, 0x20);
        exit_code = rom_state[7] >> 24;
        if (exit_code != 0 && exit_code != 0x80 && exit_code != 0xFF) break;

        safety_exit++;
        if (safety_exit > 0x8000) {
            break;
        }
    }
    irq_restore(cookie);

    dc_invalidaterange((void*)(entry+0x00000004), sizeof(u32)*4);
    dc_invalidaterange(rom_state, 0x20);
    dc_invalidaterange(body, sizeof(u32));
    dc_invalidaterange(0x100, sizeof(u32));

    printf("PPC: ROM success? %08x timer=%08lX err=0x%08lX 0x%08lX 0x%08lX spr=%08lX fuse=%08lX, magic=%08lX, test=%08lX %08lX.\n", safety_exit, rom_state[4], rom_state[7], old, *body, *(u32*)(entry+0x00000004), *(u32*)(entry+0x00000008), *(u32*)(entry+0x0000000C), *(u32*)(entry+0x00000010), *(u32*)0x100);

    u32 fuse_val = *(u32*)(entry+0x00000008);
    u32 bootrom_val = *(u32*)(entry+0x00000010);
    if (fuse_val != 0xFFFFFFFF && fuse_val != 0x0BADBABE) {
        return 1;
    }
    if (bootrom_val != 0xFFFFFFFF && bootrom_val != 0x0) {
        return 1;
    }

    //printf("PPC: PowerPC is waiting for entry!\n");
    return 0;
}

void ppc_test(u32 val)
{
    //ancast_ppc_load("slc:/sys/title/00050010/1000400a/code/kernel.img");
    //memcpy((void*)0x01330000, (void*)0x08000000, 0x11e100);

    //for (int i = 0x170680; i < 0x170700; i += 0x1)

    // 0x8CDus - bss wiped
    // 0x8CDus - flags/OTP are parsed
    // 0x1700us - almost copying, verifying
    // 0x12700us - start copying
    // 0x18700us - done verifying

    // just SRESET, no watcher
    // 0x216e0+?? loops - bss wiped
    // 0x22fe0 - winner
    // 0x5b9c00 loops - limbo
    // 0x5bd6c6 loops - OTP winner
    // 0x5bff00 loops - decrypted, verified, timer written

    // with watcher
    // 0x1716D0 done decrypting weird limbo
    // 0x1742d0 wtf, bootrom dumpable, spr=0xc0000000
    // 0x1743d0 end limbo, execute payload w/ SRESET, spr=0x80600000


    for (int i = 0x22fd0; i < 0x22fe0+0x100; i += 0x1)
    {
        printf("iteration: %02x\n", i);
        if (_ppc_test(val, i)) break;
    }
    
}