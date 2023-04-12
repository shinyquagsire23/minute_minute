/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Haxx Enterprises <bushing@gmail.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2009          Andre Heider "dhewg" <dhewg@wiibrew.org>
 *  Copyright (C) 2009          John Kelley <wiidev@kelley.ca>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "main.h"

#include "types.h"
#include "utils.h"
#include "latte.h"
#include "sdcard.h"
#include "mlc.h"
#include "string.h"
#include "memory.h"
#include "gfx.h"
#include "elm.h"
#include "irq.h"
#include "exception.h"
#include "crypto.h"
#include "nand.h"
#include "sdhc.h"
#include "dump.h"
#include "isfs.h"
#include "smc.h"
#include "filepicker.h"
#include "ancast.h"
#include "minini.h"
#include "gpio.h"
#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "ppc_elf.h"
#include "ppc.h"
#include "dram.h"
#include "smc.h"
#include "prsh.h"

static struct {
    int mode;
    u32 vector;
} boot = {0};

bool autoboot = false;
u32 autoboot_timeout_s = 3;
char autoboot_file[256] = "ios.img";

int main_autoboot(void);

extern char sd_read_buffer[0x200];

#ifdef MINUTE_BOOT1
extern otp_t otp;

#define PFLAG_10            (0x00000010)
#define PFLAG_10000         (0x00010000)
#define PON_SMC_TIMER       (0x00020000)
#define PFLAG_40000         (0x00040000)
#define CMPT_RETSTAT1       (0x00080000)
#define CMPT_RETSTAT0       (0x00100000)
#define PFLAG_DDR_SREFRESH  (0x00200000)
#define POFF_TMR            (0x00400000)
#define POFF_4S             (0x00800000)
#define POFF_FORCED         (0x01000000)
#define PON_TMR             (0x02000000)
#define PON_SMC             (0x04000000)
#define PON_WAKEREQ1_EVENT  (0x08000000)
#define PON_WAKEREQ0_EVENT  (0x10000000)
#define PON_WAKEBT_EVENT    (0x20000000)
#define PON_EJECT_BTN       (0x40000000)
#define PON_POWER_BTN       (0x80000000)

u32 _main(void *base)
{
    (void)base;
    int res = 0; (void)res;

    gfx_clear(GFX_ALL, BLACK);
    printf("minute loading\n");

    // Same as boot1:
    srand(read32(LT_TIMER));
    // boot0 already disabled
    set32(LT_RESETS_COMPAT, RSTB_IOMEM | RSTB_IOPI);
    set32(LT_EXICTRL, 1);
    gpio_fan_set(1);
    gpio_smc_i2c_init();

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    irq_initialize();
    printf("Interrupts initialized\n");

    // Adjust IOP clock multiplier to 1x
    if (read32(LT_IOP2X) & 0x04)
    {
        latte_set_iop_clock_mult(1);
    }

    // Read OTP and SEEPROM
    crypto_initialize();
    printf("crypto support initialized\n");

    serial_force_terminate();
    udelay(500000);

    // Signal binary printing
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0xBEEFCAFE);
    udelay(500000);


    // Show a little flourish to indicate we have code exec
    for (int i = 0; i < 5; i++)
    {
        smc_set_cc_indicator(LED_ON);
        udelay(50000);
        smc_set_on_indicator(LED_ON);
        udelay(50000);
    }

    // 0x00020721
    u32 rtc_ctrl0 = smc_get_ctrl0();
    //serial_send_u32(rtc_ctrl0);

    u32 debug_val = read32(LT_DEBUG);
    u32 pflags_val = 0;

    // Check if the CMPT_RETSTAT0 flag is raised
    if (debug_val & 0x04)
        pflags_val = CMPT_RETSTAT0;

    // Check if the CMPT_RETSTAT1 flag is raised
    if (debug_val & 0x08)
        pflags_val |= CMPT_RETSTAT1;

    // Check if the POFFLG_FPOFF flag is raised
    if (rtc_ctrl0 & CTRL0_POFFLG_FPOFF)
        pflags_val |= POFF_FORCED;

    // Check if the POFFLG_4S flag is raised
    if (rtc_ctrl0 & CTRL0_POFFLG_4S)
        pflags_val |= POFF_4S;

    // Check if the POFFLG_TMR flag is raised
    if (rtc_ctrl0 & CTRL0_POFFLG_TMR)
        pflags_val |= POFF_TMR;

    // Check if the PONLG_TMR flag is raised
    if (rtc_ctrl0 & CTRL0_PONLG_TMR)
        pflags_val |= PON_TMR;

    // Check if PONFLG_SYS is raised
    if (rtc_ctrl0 & CTRL0_PONFLG_SYS)
    {
        pflags_val |= PON_SMC;

        u32 sys_event = smc_get_events();

        // POWER button was pressed
        if (sys_event & SMC_POWER_BUTTON)
            pflags_val |= PON_POWER_BTN;

        // EJECT button was pressed
        if (sys_event & SMC_EJECT_BUTTON)
            pflags_val |= PON_EJECT_BTN;

        // Wake 1 signal is active
        if (sys_event & SMC_WAKE1)
            pflags_val |= PON_WAKEREQ1_EVENT;

        // Wake 0 signal is active
        if (sys_event & SMC_WAKE0)
            pflags_val |= PON_WAKEREQ0_EVENT;

        // BT interrupt request is active
        if (sys_event & SMC_BT_IRQ)
            pflags_val |= PON_WAKEBT_EVENT;

        // Timer signal is active
        if (sys_event & SMC_TIMER)
            pflags_val |= PON_SMC_TIMER;
    }

    // Raise POFFLG_TMR, PONFLG_SYS and some unknown flags
    smc_set_ctrl0(CTRL0_POFFLG_TMR | CTRL0_PONFLG_SYS | CTRL0_FLG_800000 | CTRL0_FLG_400000);

    u32 rtc_ctrl1 = smc_get_ctrl1();

    // Check if SLEEP_EN is raised
    if (rtc_ctrl1 & 0x00000100)
        pflags_val |= PFLAG_DDR_SREFRESH; // Set DDR_SREFRESH power flag

    u32 mem_mode = 0;

    // DDR_SREFRESH power flag is set
    if (pflags_val & PFLAG_DDR_SREFRESH)
        mem_mode = DRAM_MODE_SREFRESH;

    // Standby Mode boot doesn't need fans
    if (pflags_val & PON_SMC_TIMER)
    {
        // Pulse the CCIndicator
        smc_set_cc_indicator(LED_PULSE);

        mem_mode |= DRAM_MODE_CCBOOT;

        // Set FanSpeed state
        gpio_fan_set(0);
    }
    else
    {
        // Pulse the ONIndicator
        smc_set_on_indicator(LED_PULSE);
    }

    serial_send_u32(0x4D454D32); // MEM2

    // Init DRAM
    init_mem2(mem_mode);
    udelay(500000);

    // Test that DRAM is working/refreshing correctly
    int is_good = 1;
    for (int i = 0; i < 0x10; i++)
    {
        if ( i )
        {
            int v12 = (pflags_val & PFLAG_DDR_SREFRESH) ? mem_clocks_related_3(mem_mode | DRAM_MODE_40 | DRAM_MODE_20 | DRAM_MODE_4) : 0;
            if ( v12 | mem_clocks_related_3(mem_mode | DRAM_MODE_40 | DRAM_MODE_20 | DRAM_MODE_SREFRESH) )
            {
                is_good = 0;
                break;
            }
        }
        for (u32 j = 0x10000000; j != 0x10000400; j += 8 )
        {
            *(vu32*)j = 0x12345678;
            *(vu32*)(j + 4) = 0x9ABCDEF0;
        }

        is_good = 1;
        for (u32 j = 0x10000000; j != 0x10000400; j += 8 )
        {
            if (*(vu32*)j != 0x12345678) {
                //serial_send_u32(j);
                //serial_send_u32(*(vu32*)j);
                is_good = 0;
                break;
            }
            if (*(vu32*)(j + 4) != 0x9ABCDEF0) {
                //serial_send_u32(j+4);
                //serial_send_u32(*(vu32*)(j+4));
                is_good = 0;
                break;
            }
        }
        if (is_good) break;
    }

    if (!is_good) {
        serial_fatal();
    }

    serial_send_u32(0x4D454D30); // MEM0

    // Clear all MEM0
    memset(0x08000000, 0, 0x002E0000);

    // Standby Mode boot doesn't need to upclock
    if (!(pflags_val & PON_SMC_TIMER))
    {
        // Adjust IOP clock multiplier to 3x
        if (!(read32(LT_IOP2X) & 0x04))
        {
            latte_set_iop_clock_mult(3);
        }
    }
    serial_send_u32(0x50525348); // PRSH

    // TODO: PRSH?
    prsh_decrypt();
    prsh_reset();
    prsh_init();

    serial_send_u32(0x5D5D0001);
    printf("Initializing SD card...\n");
    sdcard_init();
    sdcard_ack_card();
    serial_send_u32(0x5D5D0002);

    memset(sd_read_buffer, 0x80, sizeof(sd_read_buffer));
    sdcard_read(0, 1, sd_read_buffer);
    serial_send_u32(0x5D5D0003);

    // PON_SMC_TIMER and an unknown power flag are set
    if (pflags_val & (PON_SMC_TIMER | PFLAG_10))
    {
        // Set DcdcPowerControl2 GPIO's state
        gpio_dcdc_pwrcnt2_set(0);

        smc_set_cc_indicator(LED_ON);
    }
    else
    {
        // Turn on ODDPower via SMC
        smc_set_odd_power(1);

        // Set DcdcPowerControl2 GPIO's state
        gpio_dcdc_pwrcnt2_set(1);

        smc_set_on_indicator(LED_ON);
    }

    serial_send_u32(pflags_val);
    //*(u32*)0x10008008 = pflags_val;
    //prsh_recompute_checksum();

    // Jump to payload...

    u8* test_otp = (u8*)&otp;

    boot.vector = ancast_iop_load_from_raw_sector(0x80);

    serial_send_u32(0x5D5D0004);
    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        smc_set_cc_indicator(LED_PULSE);
        while (1) {
            serial_send_u32(0xF00FAAAA);
            serial_send(sd_read_buffer[0]);
            serial_send(sd_read_buffer[1]);
            serial_send(sd_read_buffer[2]);
            serial_send(sd_read_buffer[3]);
        }
    }

    //*(vu32*)0x0d8b0820 = 0xFFFFFFFF;
    /*for (int i = 0; i < 0x400; i += 4)
    {
        *(vu32*)(0x0d8b0c00 + i) = 0x80000000;
    }*/

#if 0
    *(vu32*)0x0d8b0808 = 0x8000FFFF;

    serial_send_u32(*(vu16*)0x0d8b420a);
    serial_send_u32(*(vu16*)0x0d8b420c);
    serial_send_u32(*(vu16*)0x0d8b420e);

    serial_send_u32(0xF00FCAF1);
    *(u32*)0x10120000 = 0xE12FFF1E;
    serial_send_u32(*(u32*)0x10120000);
    
    void (*test_ram_jump)() = (void*)0x10120000;
    test_ram_jump();
    serial_send_u32(0xF00FCAF2);
#endif

    dc_flushall();
    ic_invalidateall();

    printf("Shutting down SD card...\n");
    sdcard_exit();

    printf("Shutting down interrupts...\n");
    irq_shutdown();

    printf("Shutting down caches and MMU...\n");
    mem_shutdown();

    switch(boot.mode) {
        case 0:
            if(boot.vector) {
                printf("Vectoring to 0x%08lX...\n", boot.vector);
            } else {
                printf("No vector address, hanging!\n");
                smc_set_cc_indicator(LED_PULSE);
                /*while (1) {
                    serial_send_u32(0xF00FAAAB);
                }
                panic(0);*/
            }
            break;
        //case 1: smc_power_off(); break;
        //case 2: smc_reset(); break;
    }
    

#if 0
    while (1) {
        //gpio_debug_send(0xFF);
        //udelay(500000);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        serial_send(0x55);
        serial_send(0xAA);
        serial_send(0x55);
        serial_send(0xAA);

        //serial_force_terminate();
#if 0
        for (int i = 0; i < sizeof(otp_t); i++)
        {
            u8 val = test_otp[i];

            serial_send(val);
        }
#endif
        //gpio_debug_send(0xFF);
        //udelay(500000);

        serial_send(0x99);
        serial_send(sd_read_buffer[0]);
        serial_send(sd_read_buffer[1]);
        serial_send(sd_read_buffer[2]);
        serial_send(sd_read_buffer[3]);
    }
#endif

#if 1
    //while (1)
    {
        // Signal normal printing
        /*serial_send_u32(0x55AA55AA);
        serial_send_u32(0x55AA55AA);

        serial_send_u32(0xF00FCAFE);
        serial_send_u32(boot.vector);
        serial_send_u32(*(u32*)boot.vector);*/
    }
#endif

    return boot.vector;
}
#else // MINUTE_BOOT1

u32 _main(void *base)
{
    (void)base;
    int res = 0; (void)res;

    // Signal normal printing
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0xF00FCAFE);

    gfx_clear(GFX_ALL, BLACK);
    printf("minute loading\n");

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    irq_initialize();
    printf("Interrupts initialized\n");

    prsh_reset();
    prsh_init();

    srand(read32(LT_TIMER));
    crypto_initialize();
    printf("crypto support initialized\n");
    printf("BSP version: %08x %04x %04x\n", latte_get_hw_version(), seeprom.bc.board_type, seeprom.bc.board_revision);

    printf("Initializing SD card...\n");
    sdcard_init();

    printf("Mounting SD card...\n");
    res = ELM_Mount();
    if(res) {
        printf("Error while mounting SD card (%d).\n", res);
        panic(0);
    }

    minini_init();

    FILE* otp_file = fopen("sdmc:/otp.bin", "rb");
    if (otp_file)
    {
        fread(&otp, sizeof(otp), 1, otp_file);
        fclose(otp_file);
    }

#if 0
    while (1)
    {
        printf("\nOTP:\n");
        for (int i = 0; i < 0x400; i += 4)
        {
            if (i && i % 16 == 0) {
                printf("\n");
            }
            printf("%08x", *(u32*)(((u32)&otp) + i));
        }
    }
#endif
    
    printf("Initializing MLC...\n");
    mlc_init();

    if(mlc_check_card() == SDMMC_NO_CARD) {
        printf("Error while initializing MLC.\n");
        //panic(0);
    }
    mlc_ack_card();

    printf("Mounting SLC...\n");
    //isfs_init();

    autoboot = 1;
    autoboot_timeout_s = 0;

#if 0
    // Prompt user to skip autoboot, time = 0 will skip this.
    if(autoboot)
    {
        while((autoboot_timeout_s-- > 0) && autoboot)
        {
            printf("Autobooting in %d seconds...\n", (int)autoboot_timeout_s + 1);
            printf("Press the POWER button or EJECT button to skip autoboot.\n");
            for(u32 i = 0; i < 1000000; i += 100000)
            {
                // Get input at .1s intervals.
                u8 input = smc_get_events();
                udelay(100000);
                if((input & SMC_EJECT_BUTTON) || (input & SMC_POWER_BUTTON))
                    autoboot = false;
            }
        }
    }
    
    // Try to autoboot if specified, if it fails just load the menu.
    if(autoboot && main_autoboot() == 0)
        printf("Autobooting...\n");
    else
    {
        smc_get_events();
        smc_set_odd_power(false);

        menu_init(&menu_main);

        smc_get_events();
        smc_set_odd_power(true);
    }
#endif
    main_autoboot();

    printf("Unmounting SLC...\n");
    //isfs_fini();

    printf("Shutting down MLC...\n");
    mlc_exit();
    
    printf("Shutting down SD card...\n");
    ELM_Unmount();
    sdcard_exit();

    printf("Shutting down interrupts...\n");
    irq_shutdown();

    printf("Shutting down caches and MMU...\n");
    mem_shutdown();

    switch(boot.mode) {
        case 0:
            if(boot.vector) {
                printf("Vectoring to 0x%08lX...\n", boot.vector);
            } else {
                printf("No vector address, hanging!\n");
                panic(0);
            }
            break;
        case 1: smc_power_off(); break;
        case 2: smc_reset(); break;
    }

    u32* search = (u32*)0x01000200;
    for (int i = 0; i < 0x1000000; i += 4) {
        if (search[0] == 0x4F545053) {
            if (search[2] == 0x4F545053 && search[1] == 0x544F5245 && search[3] == 0x544F5245) {
                printf("OTP store at: %08x\n", (u32)search);
                memcpy((void*)search, &otp, sizeof(otp));
                break;
            }
        }
        search++;
    }

#if 0
    while (1)
    {
        printf("\nOTP:\n");
        for (int i = 0; i < 0x400; i += 4)
        {
            if (i && i % 16 == 0) {
                printf("\n");
            }
            printf("%08x", *(u32*)(((u32)&otp) + i));
        }
    }
#endif

    // WiiU-Firmware-Emulator JIT bug
    void (*boot_vector)(void) = (void*)boot.vector;
    boot_vector();

    return boot.vector;
}

int boot_ini(const char* key, const char* value)
{
    if(!strcmp(key, "autoboot"))
        autoboot = minini_get_bool(value, 0);
    if(!strcmp(key, "autoboot_file"))
        strncpy(autoboot_file, value, sizeof(autoboot_file));
    if(!strcmp(key, "autoboot_timeout"))
        autoboot_timeout_s = (u32)minini_get_uint(value, 3);
    
    return 0;
}

int main_autoboot(void)
{
    FILE* f = fopen(autoboot_file, "rb");
    if(f == NULL)
    {
        printf("Failed to open %s.\n", autoboot_file);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
        return -1;
    }

    u32 magic;
    fread(&magic, 1, sizeof(magic), f);
    fclose(f);
    
    // Ancast image.
    if(magic == 0xEFA282D9) {
        boot.vector = ancast_iop_load(autoboot_file);
    }
    
    if(boot.vector)
    {
        boot.mode = 0;
        return 0;
    }
    else
    {
        printf("Failed to load file for autoboot: %s\n", autoboot_file);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
        return -2;
    }
}

void main_reload(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("fw.img");

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load fw.img!\n");
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_shutdown(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 1;
    menu_active = false;
}

void main_reset(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 2;
    menu_active = false;
}

void main_boot_ppc(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char path[_MAX_LFN] = {0};
    pick_file("sdmc:", false, path);

    u32 entry = 0;
    int res = ppc_load_file(path, &entry);
    if(res) {
        printf("ppc_load_file: %d\n", res);
        goto ppc_exit;
    }

    ppc_jump(entry);

ppc_exit:
    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_quickboot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("ios.img");

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load 'ios.img'!\n");
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_boot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char path[_MAX_LFN] = {0};
    pick_file("sdmc:", false, path);

    boot.vector = ancast_iop_load(path);

    if(boot.vector) {
        boot.mode = 0;
        menu_active = false;
    } else {
        printf("Failed to load '%s'!\n", path);
        printf("Press POWER to continue.\n");
        smc_wait_events(SMC_POWER_BUTTON);
    }
}

void main_reset_crash(void)
{
	gfx_clear(GFX_ALL, BLACK);

	printf("Clearing SMC crash buffer...\n");

	const char buffer[64 + 1] = "Crash buffer empty.";
	smc_set_panic_reason(buffer);

    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_get_crash(void)
{
    gfx_clear(GFX_ALL, BLACK);
    printf("Reading SMC crash buffer...\n");

    char buffer[64 + 1] = {0};
    smc_get_panic_reason(buffer);

    // We use this SMC buffer for storing exception info, however, it is only 64 bytes.
    // This is exactly enough for r0-r15, but nothing else - not even some exception "magic"
    // or even exception type. Here we have some crap "heuristic" to determine if it's ASCII text
    // (a panic reason) or an exception dump.
    bool exception = false;
    for(int i = 0; i < 64; i++)
    {
        char c = buffer[i];
        if(c >= 32 && c < 127) continue;
        if(c == 10 || c == 0) continue;

        exception = true;
        break;
    }

    if(exception) {
        u32* regs = (u32*)buffer;
        printf("Exception registers:\n");
        printf("  R0-R3: %08lx %08lx %08lx %08lx\n", regs[0], regs[1], regs[2], regs[3]);
        printf("  R4-R7: %08lx %08lx %08lx %08lx\n", regs[4], regs[5], regs[6], regs[7]);
        printf(" R8-R11: %08lx %08lx %08lx %08lx\n", regs[8], regs[9], regs[10], regs[11]);
        printf("R12-R15: %08lx %08lx %08lx %08lx\n", regs[12], regs[13], regs[14], regs[15]);
    } else {
        printf("Panic reason:\n");
        printf("%s\n", buffer);
    }

    printf("Press POWER to exit.\n");
    smc_wait_events(SMC_POWER_BUTTON);
}

void main_credits(void)
{
    gfx_clear(GFX_ALL, BLACK);
    console_init();

    console_add_text("minute (not minute) - a Wii U port of mini\n");

    console_add_text("The SALT team: Dazzozo, WulfyStylez, shinyquagsire23 and Relys (in spirit)\n");

    console_add_text("Special thanks to fail0verflow (formerly Team Twiizers) for the original \"mini\", and for the vast\nmajority of Wii research and early Wii U research!\n");

    console_add_text("Thanks to all WiiUBrew contributors, including: Hykem, Marionumber1, smea, yellows8, derrek,\nplutoo, naehrwert...\n");

    console_add_text("Press POWER to exit.");

    console_show();
    smc_wait_events(SMC_POWER_BUTTON);
}
#endif // !MINUTE_BOOT1