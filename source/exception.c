/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "irq.h"
#include "gfx.h"
#include "utils.h"
#include "memory.h"
#include "serial.h"
#include "latte.h"

const char *exceptions[] = {
    "RESET", "UNDEFINED INSTR", "SWI", "INSTR ABORT", "DATA ABORT",
    "RESERVED", "IRQ", "FIQ", "(unknown exception type)"
};

const char *aborts[] = {
    "UNDEFINED",
    "Alignment",
    "UNDEFINED",
    "Alignment",
    "UNDEFINED",
    "Translation",
    "UNDEFINED",
    "Translation",
    "External abort",
    "Domain",
    "External abort",
    "Domain",
    "External abort on translation (first level)",
    "Permission",
    "External abort on translation (second level)",
    "Permission"
};

u8 domvalid[] = {0,0,0,0,0,0,0,1,0,1,0,1,0,1,1,1};

void exc_setup_stack(void);

void exception_initialize(void)
{
    exc_setup_stack();
    u32 cr = get_cr();
    cr |= 0x2; // Data alignment fault checking enable
    set_cr(cr);
}

void exc_handler(u32 type, u32 spsr, u32 *regs)
{
    (void) spsr;

#ifdef MINUTE_BOOT1
    serial_send_u32(0xAAAAAAFE);
#else
    //serial_send_u32(0xAAAAAAFD);
#endif

    if (type > 8) type = 8;
    printf("Exception %d (%s):\n", type, exceptions[type]);

    u32 pc, fsr;

    switch(type) {
        case 1: // UND
        case 2: // SWI
        case 3: // INSTR ABORT
        case 7: // FIQ
            pc = regs[15] - 4;
            break;
        case 4: // DATA ABORT
            pc = regs[15] - 8;
            break;
        default:
            pc = regs[15];
            break;
    }

    printf("Registers (%p):\n", regs);
    printf("  R0-R3: %08x %08x %08x %08x\n", regs[0], regs[1], regs[2], regs[3]);
    printf("  R4-R7: %08x %08x %08x %08x\n", regs[4], regs[5], regs[6], regs[7]);
    printf(" R8-R11: %08x %08x %08x %08x\n", regs[8], regs[9], regs[10], regs[11]);
    printf("R12-R15: %08x %08x %08x %08x\n", regs[12], regs[13], regs[14], pc);

    printf("SPSR: %08x\n", spsr);
    printf("CPSR: %08x\n", get_cpsr());
    printf("CR:   %08x\n", get_cr());
    printf("TTBR: %08x\n", get_ttbr());
    printf("DACR: %08x\n", get_dacr());

    switch (type) {
        case 3: // INSTR ABORT
        case 4: // DATA ABORT
            if(type == 3)
                fsr = get_ifsr();
            else
                fsr = get_dfsr();
            printf("Abort type: %s\n", aborts[fsr&0xf]);
            if(domvalid[fsr&0xf])
                printf("Domain: %d\n", (fsr>>4)&0xf);
            if(type == 4)
                printf("Address: 0x%08x\n", get_far());
        break;
        default: break;
    }

    /*serial_send_u32(spsr);
    serial_send_u32(get_cpsr());
    serial_send_u32(get_cr());
    serial_send_u32(get_ttbr());
    serial_send_u32(get_dacr());*/

#ifdef MINUTE_BOOT1
    serial_send_u32(type);
    serial_send_u32(pc);
    serial_send_u32(regs[14]);
    serial_send_u32((type == 4)?get_far():0x0);
    serial_send_u32(*(u32*)(pc-4));
    serial_send_u32(*(u32*)pc);
    serial_send_u32(*(u32*)(pc+4));
#endif

    /*
    serial_send_u32(*(vu32*)LT_RESETS_AHMN);
    serial_send_u32(*(vu32*)0x0d8b0800);
    serial_send_u32(*(vu32*)0x0d8b0804);
    serial_send_u32(*(vu32*)0x0d8b0808);
    serial_send_u32(0xCAFECAFE);*/

    for (int i = 0; i < 0x700; i += 4)
    {
        //serial_send_u32(*(vu32*)(0x0d8b0900 + i));
    }

    /*
    60 00 00 5f 
    60 00 00 db 
    00 05 30 ff 
    0d 40 40 00 
    ff ff ff ff 
    00 00 00 01 
    10 12 00 00 
    00 00 00 00 
    */

    if(type != 3) {
        printf("Code dump:\n");
        printf("%08x:  %08x %08x %08x %08x\n", pc-16, read32(pc-16), read32(pc-12), read32(pc-8), read32(pc-4));
        printf("%08x: *%08x %08x %08x %08x\n", pc, read32(pc), read32(pc+4), read32(pc+8), read32(pc+12));
        printf("%08x:  %08x %08x %08x %08x\n", pc+16, read32(pc+16), read32(pc+20), read32(pc+24), read32(pc+28));
    }

    panic(0);
}
