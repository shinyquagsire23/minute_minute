/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *  Copyright (C) 2016, 2023    Max Thomas <mtinc2@gmail.com>
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
#include "serial.h"
#include "memory.h"
#include "ppc_elf.h"
#include "ppc.h"
#include "dram.h"
#include "smc.h"
#include "rtc.h"
#include "prsh.h"
#include "asic.h"
#include "gpu.h"
#include "exi.h"
#include "interactive_console.h"
#include "isfshax.h"
#include "rednand.h"
#include "isfshax_patch.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <dirent.h>

//#define MEASURE_TIME

static struct {
    int mode;
    u32 vector;
    int is_patched;
    int needs_otp;
} boot = {0};

int autoboot = 0;
u32 autoboot_timeout_s = 3;
char autoboot_file[256] = "ios.patch";
const char sd_plugin_dir[] = "sdmc:/wiiu/ios_plugins";
const char slc_plugin_dir[] = "slc:/sys/hax/ios_plugins";
int main_loaded_from_boot1 = 0;
bool minute_on_slc = false;
bool minute_on_sd = false;
int main_is_de_Fused = 0;
int main_force_pause = 0;
int main_allow_legacy_patches = 0;

int main_autoboot(void);
void main_quickboot_patch_slc(void);
void main_quickboot_patch_rednand(void);
void main_swapboot_patch_rednand(void);

extern char sd_read_buffer[0x200];

void silly_tests();

extern otp_t otp;

static u32 boot1_patch_isfshax(void){
    serial_send_u32(0x7D4D0001);
    u32 vector = ancast_iop_load("slc:/sys/title/00050010/1000400a/code/fw.img");
    if(!vector)
        return vector;
    serial_send_u32(0x7D4D0003);
    if(!isfshax_patch_apply(vector))
        return 0;
    serial_send_u32(0x7D4D0004);
    return vector;

}

#ifdef MINUTE_BOOT1

static bool read_ancast(const char *path){
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    size_t read = fread((void*)ALL_PURPOSE_TMP_BUF, 1, 0x800000, f);
    if (!read) {
        return false;
    }
    fclose(f);

    return *(u32*)ALL_PURPOSE_TMP_BUF == ANCAST_MAGIC; 
}

static bool load_fw_from_sd_fat(void){
        FRESULT res = FR_OK;
        static FATFS fatfs = {0};
        static devoptab_t devoptab = {0};
        FIL f = {0};
        unsigned int read;

        serial_send_u32(0x5D5E0004);

        res = f_mount(&fatfs, "sdmc:", 1);
        if (res != FR_OK) {
            sdcard_init(); // TODO whyyyyy
            res = f_mount(&fatfs, "sdmc:", 1);
            if (res != FR_OK) {
                return false;
            }
        }
        res = f_open(&f, "sdmc:/fw.img", FA_OPEN_EXISTING | FA_READ);
        if (res != FR_OK) {
            return false;
        }
        res = f_read(&f, (void*)ALL_PURPOSE_TMP_BUF, 0x800000, &read);
        f_close(&f);
        if (res != FR_OK) {
            return false;
        }

        serial_send_u32(0x5D5E0008);

        return *(u32*)ALL_PURPOSE_TMP_BUF == ANCAST_MAGIC;
}

static u32 load_fw_from_sd(bool retry_forever){
    u32 vector = 0;
    for(int i = 0; !vector && (i<5 || retry_forever); i++)   
    {
        if(i==1)
            smc_set_notification_led(LEDRAW_ORANGE_PULSE);
        serial_send_u32(0x5D5D0001);
        printf("Initializing SD card...\n");
        sdcard_init();
        serial_send_u32(0x6D6D0001);

        int loaded_from_fat = load_fw_from_sd_fat();
        if (loaded_from_fat) {
            vector = ancast_iop_load_from_memory((void*)ALL_PURPOSE_TMP_BUF);
        }
        else {
            vector = ancast_iop_load_from_raw_sector(0x80);
        }
        
        serial_send_u32(0x5D5D0004);
    }
    printf("Shutting down SD card...\n");
    sdcard_exit();
    return vector;
}

boot_info_t *init_prsh_get_bootinfo(void) {
    serial_send_u32(0x50525348); // PRSH
    // Set up PRSH here
    // prsh_decrypt();
    prsh_reset();
    prsh_init();
    boot_info_t *boot_info;
    size_t boot_info_size;
    int res = prsh_get_entry("boot_info", (void**) &boot_info, &boot_info_size);
    if(!res && boot_info_size >= sizeof(boot_info_t)){
        return boot_info;
    }
    return NULL;
}

u32 _main(void *base)
{
    (void)base;
    int res = 0; (void)res;

    gfx_init();
    exi_init();
    printf("minute loading\n");

    serial_force_terminate();
    udelay(500);

    // Signal binary printing
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0xBEEFCAFE);
    udelay(500);

    // Same as boot1:
    //srand(read32(LT_TIMER));
    // boot0 already disabled
    set32(LT_RESETS_COMPAT, RSTB_IOMEM | RSTB_IOPI);
    set32(LT_EXICTRL, 1);
    gpio_fan_set(1);
    gpio_fanspeed_set(FANSPEED_FAST);
    gpio_smc_i2c_init();

    // boot0 SDcard regpokes
    {
        gpio_enable(30, 1);
    }

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    // Adjust IOP clock multiplier to 1x
    if (read32(LT_IOP2X) & 0x04)
    {
        latte_set_iop_clock_mult(1);
    }

    // Read OTP and SEEPROM
    crypto_initialize();
    printf("crypto support initialized\n");

    u32 pflags_val = 0;

#ifndef ISFSHAX_STAGE2
    // 0x00020721 on extra cold booting (fresh from power plug)
    // 0x02020321 on warm cold booting (or 0x02000221?)
    u32 rtc_ctrl0 = rtc_get_ctrl0();
    serial_send_u32(rtc_ctrl0);

    u32 syscfg1_val = read32(LT_SYSCFG1);

    // Check if the CMPT_RETSTAT0 flag is raised
    if (syscfg1_val & 0x04)
        pflags_val = CMPT_RETSTAT0;

    // Check if the CMPT_RETSTAT1 flag is raised
    if (syscfg1_val & 0x08)
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
    rtc_set_ctrl0(CTRL0_POFFLG_TMR | CTRL0_PONFLG_SYS | CTRL0_FLG_00800000 | CTRL0_FLG_00400000);

    u32 rtc_ctrl1 = rtc_get_ctrl1();

    // Check if SLEEP_EN is raised
    if (rtc_ctrl1 & CTRL1_SLEEP_EN)
        pflags_val |= PFLAG_DDR_SREFRESH; // Set DDR_SREFRESH power flag

    serial_send_u32(0x464C4147);
    serial_send_u32(pflags_val);
    serial_send_u32(0x464C4147);

#else 
    // ISFSHAX_STAGE2
    // on ISFShax the flags were already extracted by boot1
    boot_info_t *bootinfo = init_prsh_get_bootinfo();
    if(bootinfo)
        pflags_val = bootinfo->boot_state;
#endif
    serial_send_u32(pflags_val);

    // Show a little flourish to indicate we have code exec
    if (pflags_val & PON_SMC_TIMER)
    {
        for (int i = 0; i < 5; i++)
        {
            smc_set_notification_led(LEDRAW_ORANGE);
            udelay(10000);
            smc_set_notification_led(LEDRAW_YELLOW);
            udelay(10000);
            smc_set_notification_led(LEDRAW_ORANGE);
            udelay(10000);
        }
    }
    else
    {
        for (int i = 0; i < 5; i++)
        {
            smc_set_notification_led(LEDRAW_ORANGE);
            udelay(10000);
            smc_set_notification_led(LEDRAW_YELLOW);
            udelay(10000);
            smc_set_notification_led(LEDRAW_NOTGREEN);
            udelay(10000);
            smc_set_notification_led(LEDRAW_BLUE);
            udelay(10000);
            smc_set_notification_led(LEDRAW_PURPLE);
            udelay(10000);
        }
    }
    

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
        gpio_fanspeed_set(FANSPEED_SLOW);
    }
    else
    {
        // Pulse the ONIndicator
        //smc_set_on_indicator(LED_PULSE);
        smc_set_notification_led(LEDRAW_PURPLE_PULSE);
    }

    serial_send_u32(seeprom.bc.board_type);
    serial_send_u32(latte_get_hw_version());
    serial_send_u32(0x4D454D32); // MEM2

#ifndef ISFSHAX_STAGE2
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
                is_good = 0;
                break;
            }
            if (*(vu32*)(j + 4) != 0x9ABCDEF0) {
                is_good = 0;
                break;
            }
        }
        if (is_good) break;
    }

    if (!is_good) {
        serial_fatal();
    }
#endif //NOT ISFSHAX_STAGE2

    serial_send_u32(0x4D454D30); // MEM0

    // Clear all MEM0
    memset((void*)0x08000000, 0, 0x002E0000);

    // Standby Mode boot doesn't need to upclock
    if (!(pflags_val & PON_SMC_TIMER))
    {
        // Adjust IOP clock multiplier to 3x
        if (!(read32(LT_IOP2X) & 0x04))
        {
            latte_set_iop_clock_mult(3);
        }
    }

    // PON_SMC_TIMER and an unknown power flag are set
    if (pflags_val & (PON_SMC_TIMER | PFLAG_ENTER_BG_NORMAL_MODE))
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

        //smc_set_on_indicator(LED_ON);
        smc_set_notification_led(LEDRAW_PURPLE);
    }

    bool slc_mounted = false;

#ifdef ISFSHAX_STAGE2
    //Skip ISFS boot by pressing power
    if (!(smc_get_events() & SMC_POWER_BUTTON && !(pflags_val & (CMPT_RETSTAT0|CMPT_RETSTAT1)))) {
        serial_send_u32(0x5D4D0001);
        printf("Mounting SLC...\n");
        irq_initialize();
        isfs_init(ISFSVOL_SLC);
        slc_mounted = true;
        serial_send_u32(0x5D4D0003);
        int res = isfshax_refresh();
        prsh_add_entry("isfshax_refresh", (void*)res, 0, NULL);
        serial_send_u32(0x5D4D0004);
        bool ok = read_ancast("slc:/sys/hax/fw.img");
        if(ok)
            boot.vector = ancast_iop_load_from_memory((void*)ALL_PURPOSE_TMP_BUF);
        serial_send_u32(0x5D4D0005);
        if(boot.vector){
            boot.mode = 0;
            menu_reset();
            minute_on_slc = true;
            serial_send_u32(0x5D4D006);
        }
    }
    #endif //ISFSHAX_STAGE2 
    
    if(!boot.vector)
        boot.vector = load_fw_from_sd(!slc_mounted);

#ifdef ISFSHAX_STAGE2
    if(slc_mounted){
        if(!boot.vector) {
            serial_send_u32(0x5D4D0007);
            boot.vector = boot1_patch_isfshax();
        }
        isfs_fini();
        irq_shutdown();
        serial_send_u32(0x5D4D0008);
    }
#endif

    if(boot.vector){
        boot.mode = 0;
        menu_reset();
    }
boot:
    serial_send_u32(0x6D6D0001);
    // Reset LED to purple if SD card is successful.
    if (!(pflags_val & (PON_SMC_TIMER | PFLAG_ENTER_BG_NORMAL_MODE)))
    {
        smc_set_notification_led(LEDRAW_PURPLE);
    }
    serial_send_u32(0x6D6D0002);
    dc_flushall();
    ic_invalidateall();
    serial_send_u32(0x6D6D0003);
    printf("Shutting down caches and MMU...\n");
    mem_shutdown();
    serial_send_u32(0x6D6D0004);
    switch(boot.mode) {
        case 0:
            if(boot.vector) {
                printf("Vectoring to 0x%08lX...\n", boot.vector);
            } else {
                printf("No vector address, hanging!\n");
                smc_set_notification_led(LEDRAW_ORANGE_PULSE);
                panic(0);
            }
            break;
        //case 1: smc_power_off(); break;
        //case 2: smc_reset(); break;
    }
    serial_send_u32(0x6D6D0005);
    // Let minute know that we're launched from boot1
    memcpy(BOOT1_PASSALONG->magic, PASSALONG_MAGIC_BOOT1, 8);
    if(minute_on_slc)
        memcpy(BOOT1_PASSALONG->magic_device, PASSALONG_MAGIC_DEVICE_SLC, 8);
    else
        memcpy(BOOT1_PASSALONG->magic_device, PASSALONG_MAGIC_DEVICE_SD, 8);

#ifdef ISFSHAX_STAGE2
    memcpy(BOOT1_PASSALONG->magic_prsh, PASSALONG_MAGIC_PRSH_DECRYPTED, 8);

    if(bootinfo) {
        memcpy(&BOOT1_PASSALONG->boot_info, bootinfo, sizeof(boot_info_t));
    }
    else {
        // TODO: fill in the rest of boot_info correctly
        prsh_copy_default_bootinfo(&BOOT1_PASSALONG->boot_info);
    }
#else
    memcpy(BOOT1_PASSALONG->magic_prsh, PASSALONG_MAGIC_PRSH_ENCRYPTED, 8);

    // TODO: fill in the rest of boot_info correctly
    prsh_copy_default_bootinfo(&BOOT1_PASSALONG->boot_info);
#endif

    BOOT1_PASSALONG->boot_info.boot_state = pflags_val;
    serial_send_u32(0x6D6D00FF);
    return boot.vector;
}
#else // MINUTE_BOOT1

void main_swapboot_patch(void);
void main_quickboot_isfshax(void);

#ifndef FASTBOOT
menu menu_main = {
    "minute", // title
    {
            "Main menu", // subtitles
    },
    1, // number of subtitles
    {
        {"Patch (slc) and boot IOS (slc)", &main_quickboot_patch_slc},
        {"Patch (sd) and boot IOS redNAND", &main_quickboot_patch_rednand},
        {"Patch (sd) and boot IOS (slc)", &main_quickboot_patch}, // options
        {"Patch (sd) and boot sdmc:/ios_orig.img", &main_swapboot_patch}, // options
        {"Patch (sd) and boot sdmc:/ios_orig.img redNAND", &main_swapboot_patch_rednand},
        {"Patch ISFShax and boot IOS (slc)", &main_quickboot_isfshax},
        {"Boot 'ios.img'", &main_quickboot_fw},
        {"Boot IOP firmware file", &main_boot_fw},
        {"Boot PowerPC ELF file", &main_boot_ppc},
        {"Backup and Restore", &dump_menu_show},
        {"Interactive debug console", &main_interactive_console},
        {"PRSH tweaks", &prsh_menu},
        {"Display crash log", &main_get_crash},
        {"Clear crash log", &main_reset_crash},
        {"Restart minute", &main_reload},
        {"Hardware reset", &main_reset},
        {"Power off", &main_shutdown},
        {"Credits", &main_credits},
        //{"ISFS test", &isfs_test},
    },
    18, // number of options
    0,
    0
};
#endif // !FASTBOOT

u32 _main(void *base)
{
    (void)base;
    int res = 0; (void)res;
    int has_no_otp_bin = 0;
    int prsh_is_encrypted = 0;
    int boot_info_source = 0;
    void *minute_on_slc_tmp;
    size_t minute_on_slc_size;
    boot_info_t *boot_info;
    size_t boot_info_size;
    boot_info_t boot_info_copy;
#ifdef FASTBOOT
    bool no_gpu = true;
    printf("FASTBOOT MODE!\n");
#else
    bool no_gpu = false;
#endif
    bool no_menu = no_gpu;
#ifdef MEASURE_TIME
    u32 minute_start_time = read32(LT_TIMER);
#endif

    write32(LT_SRNPROT, 0x7BF);
    exi_init();

    // Signal normal printing
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0x55AA55AA);
    serial_send_u32(0xF00FCAFE);

    prsh_copy_default_bootinfo(&boot_info_copy);

    // Grab boot_info and anything else important from minute_boot1, if we detect it was run
    if (!memcmp(BOOT1_PASSALONG->magic, PASSALONG_MAGIC_BOOT1, 8)) {
        main_loaded_from_boot1 = 1;
        memset(BOOT1_PASSALONG->magic, 0, 8);

        if (!memcmp(BOOT1_PASSALONG->magic_device, PASSALONG_MAGIC_DEVICE_SLC, 8)) {
            minute_on_slc = true;
            memset(BOOT1_PASSALONG->magic_device, 0, 8);
        } else if (!memcmp(BOOT1_PASSALONG->magic_device, PASSALONG_MAGIC_DEVICE_SD, 8)) {
            minute_on_sd = true;
            memset(BOOT1_PASSALONG->magic_device, 0, 8);
        }

        if (!memcmp(BOOT1_PASSALONG->magic_prsh, PASSALONG_MAGIC_PRSH_ENCRYPTED, 8))
        {
            prsh_is_encrypted = 1;
        }

        if (!memcmp(BOOT1_PASSALONG->magic_prsh, PASSALONG_MAGIC_PRSH_ENCRYPTED, 8) || !memcmp(BOOT1_PASSALONG->magic_prsh, PASSALONG_MAGIC_PRSH_DECRYPTED, 8)) {
            memcpy(&boot_info_copy, &BOOT1_PASSALONG->boot_info, sizeof(boot_info_copy));
            boot_info_source = 1;

            memset(BOOT1_PASSALONG->magic_prsh, 0, sizeof(u32));
            memset(&BOOT1_PASSALONG->boot_info, 0, sizeof(boot_info_t));
        }
        else if (prsh_exists_decrypted()) {
            prsh_is_encrypted = 0;

            prsh_reset();
            prsh_init();

            res = prsh_get_entry("boot_info", (void**)&boot_info, &boot_info_size );
            if(!res){
                memcpy(&boot_info_copy, boot_info, sizeof(boot_info_copy));
                boot_info_source = 2;
            }
        }
    }
    else {
        if (prsh_exists_decrypted()) {
            prsh_is_encrypted = 0;

            prsh_reset();
            prsh_init();

            res = prsh_get_entry("boot_info", (void**)&boot_info, &boot_info_size );
            if(!res){
                memcpy(&boot_info_copy, boot_info, sizeof(boot_info_copy));
                boot_info_source = 3;
            }
        }
        else {
            prsh_is_encrypted = 1;
        }
    }

    if (read32(MAGIC_PLUG_ADDR) == MAGIC_PLUG)
    {
        write32(MAGIC_PLUG_ADDR, 0xBADBADBA);
    }

    printf("boot_state: %X\n", boot_info_copy.boot_state);
 
    bool is_eco_mode = boot_info_copy.boot_state & PON_SMC_TIMER;
    if(is_eco_mode) {
        printf("ECO Mode!\n");
        no_menu = no_gpu = true;
    }

    bool is_iosu_reload = boot_info_copy.boot_state & PFLAG_PON_RELOAD;
    if(is_iosu_reload){
        no_gpu = true;
        if(prsh_exists_decrypted()){
            res = prsh_get_entry("minute_boot", (void**)&autoboot, NULL );
            if(!res && autoboot){
                printf("IOSU Reload! autobooting %d...\n", autoboot);
                // IOSU reload, gpu is already inited by IOSU
                no_menu = true;
            }
        }
    }
#ifdef MEASURE_TIME
    u32 graphic_start = read32(LT_TIMER);
#endif
    if(!no_gpu) {
        gpu_display_init();
        gfx_init();
    }
#ifdef MEASURE_TIME
    u32 graphic_end = read32(LT_TIMER);
#endif

    printf("minute loading\n");

    if (main_loaded_from_boot1) {
        printf("minute was loaded from boot1 context!\n");
    }

    // Useful debug info
    if (boot_info_source == 0) {
        printf("boot_info source: default values\n");
    }
    else if (boot_info_source == 1) {
        printf("boot_info source: set by minute_boot1\n");
    }
    else if (boot_info_source == 2) {
        printf("boot_info source: pre-decrypted PRSH from boot1\n");
    }
    else if (boot_info_source == 3) {
        printf("boot_info source: pre-decrypted PRSH not from boot1\n");
    }

    printf("Initializing exceptions...\n");
    exception_initialize();
    printf("Configuring caches and MMU...\n");
    mem_initialize();

    irq_initialize();
    printf("Interrupts initialized\n");

    srand(read32(LT_TIMER));
    crypto_initialize();
    printf("crypto support initialized\n");
    latte_print_hardware_info();

    printf("Initializing SD card...\n");
#ifdef MEASURE_TIME
    u32 sd_start = read32(LT_TIMER);
    u32 sd_end = sd_start;
#endif
#ifndef FASTBOOT
    sdcard_init();
    printf("sdcard_init finished\n");

    printf("Mounting SD card...\n");
    res = ELM_Mount();
    if(res) {
        printf("Error while mounting SD card (%d).\n", res);
    }
#ifdef MEASURE_TIME
    sd_end = read32(LT_TIMER);
#endif

    crypto_check_de_Fused();


    // Write out our dumped OTP, if valid
    if (read32(PRSHHAX_OTPDUMP_PTR) == PRSHHAX_OTP_MAGIC) {
        write32(PRSHHAX_OTPDUMP_PTR, 0);
        FILE* f_otp = fopen("sdmc:/otp.bin", "wb");
        if (f_otp)
        {
            fwrite((void*)(PRSHHAX_OTPDUMP_PTR+4), sizeof(otp), 1, f_otp);
            fclose(f_otp);

            printf("OTP dumped successfully and was written to `sdmc:/otp.bin`.\n");

            console_power_to_continue();
        }
        else {
            printf("OTP dumped, but couldn't open `sdmc:/otp.bin`!\n");

            u8* otp_iter = (u8*)(PRSHHAX_OTPDUMP_PTR+4);
            for (int i = 0; i < 0x400; i++)
            {
                if (i && i % 16 == 0) {
                    printf("\n");
                }
                printf("%02x ", *otp_iter++);
            }
            printf("\n");

            console_power_to_continue();
        }

        memcpy(&otp, (void*)(PRSHHAX_OTPDUMP_PTR+4), sizeof(otp));
        has_no_otp_bin = 0;
    } else if (read32(PRSHHAX_OTPDUMP_PTR) == PRSHHAX_FAIL_MAGIC) {
        write32(PRSHHAX_OTPDUMP_PTR, 0);
        printf("boot1 never jumped to payload! Offset or SEEPROM version might be incorrect.\n");
        printf("(try it again just in case, sometimes the resets can get weird)\n");

        console_power_to_continue();
    }

    if (crypto_otp_is_de_Fused)
    {
        //console_power_to_continue();

        printf("Console is de_Fused! Loading sdmc:/otp.bin...\n");
        FILE* otp_file = fopen("sdmc:/otp.bin", "rb");
        if (otp_file)
        {
            fread(&otp, sizeof(otp), 1, otp_file);
            fclose(otp_file);
        }
        else {
            printf("Failed to load `sdmc:/otp.bin`!\nFirmware will fail to load.\n");
            has_no_otp_bin = 1;

            console_power_to_continue();
        }
    }

    // Hopefully we have proper keys by this point
    crypto_decrypt_seeprom();

#endif // FASTBOOT

    if (prsh_is_encrypted)
    {
        printf("prsh: decrypting.\n");
        prsh_decrypt();
    }

    prsh_reset();
    prsh_init();

#ifndef FASTBOOT
    int isfshax_refresh = 0;
    prsh_get_entry("isfshax_refresh", (void**)&isfshax_refresh, NULL);
    if(isfshax_refresh){
        print_isfshax_refresh_error(isfshax_refresh);
        console_power_to_continue();
    }
#endif

    // If we're coming from boot1 and PRSH is encrypted, the new boot_info is what we should use.
    // Otherwise, if we're not coming from boot1, copy to boot_info_copy
    if (prsh_is_encrypted && main_loaded_from_boot1)
    {
        res = prsh_get_entry("boot_info", (void**)&boot_info, &boot_info_size );
        if(!res) {
            printf("Setting boot_info to boot1 passalong.\n");
            memcpy(boot_info, &boot_info_copy, sizeof(boot_info_copy));
            print_bootinfo(boot_info);
        }
    }
    else {

        res = prsh_get_entry("boot_info", (void**)&boot_info, &boot_info_size );
        if(!res) {
            printf("boot_info exists, making copy.\n");
            memcpy(&boot_info_copy, boot_info, sizeof(boot_info_copy));
            print_bootinfo(boot_info);
        }
    }

    res = prsh_get_entry("minute_on_slc", (void**)&minute_on_slc_tmp, &minute_on_slc_size );
    if(!res){
        minute_on_slc = true;
        minute_on_sd = false;
    }
#ifdef MEASURE_TIME
    u32 ini_start = read32(LT_TIMER);
#endif
#ifndef FASTBOOT
    minini_init();
#endif
#ifdef MEASURE_TIME
    u32 ini_end = read32(LT_TIMER);
#endif

    // idk?
    if (main_loaded_from_boot1) {
        write32(0xC, 0x20008000);
    }

    if(sdcard_check_card() == SDMMC_NO_CARD){
        printf("No SD card inserted!\n");
        isfs_init(ISFSVOL_SLC);
        DIR* dir = opendir(slc_plugin_dir);
        if (dir) {
            closedir(dir);
            printf("No SD Card found, autobooting SLC\n");
            autoboot = 1;
        }
    }

    //isfs_test();

#if 0
    if (crypto_otp_is_de_Fused && has_no_otp_bin) {
        printf("Resetting into boot1 to dump ")
        dump_otp_via_prshhax();
    }
#endif

    // Can't boot w/o OTP
    if (has_no_otp_bin) {
        printf("No OTP bin, showing menu...\n");
        autoboot = false;
    }

    // ~Triple press opens menu
    if (smc_get_events() & SMC_POWER_BUTTON) {
        printf("Power button spam, showing menu...\n");
        autoboot = false;
    }
#ifdef MEASURE_TIME
    u32 init_end = read32(LT_TIMER);
#endif

#ifdef FASTBOOT
    main_quickboot_patch_slc();
#else
    // Prompt user to skip autoboot, time = 0 will skip this.
    if(autoboot)
    {
        while((autoboot_timeout_s-- > 0) && autoboot)
        {
            printf("Autobooting in %d seconds...\n", (int)autoboot_timeout_s + 1);
            printf("Press the POWER button or EJECT button to skip autoboot.\n");
            for(u32 i = 0; i < 1000000; i += 100000)
            {
                // Don't wait in ECO mode or IOSU reload.
                if (no_menu) break;

                // Get input at .1s intervals.
                u8 input = smc_get_events();
                udelay(100000);
                if((input & SMC_EJECT_BUTTON) || (input & SMC_POWER_BUTTON)) {
                    autoboot = false;
                    break;
                }
            }
        }
    }
    
    // Try to autoboot if specified, if it fails just load the menu.
    if(autoboot && main_autoboot() == 0) {
        printf("Autobooting...\n");
    }
    else
    {
        smc_get_events();
        //leave ODD Power on for HDDs
        if (has_no_otp_bin || 
                (seeprom.bc.sata_device != SATA_TYPE_GEN2HDD && 
                 seeprom.bc.sata_device != SATA_TYPE_GEN1HDD))
            smc_set_odd_power(false);

        // Shut down if in ECO mode w/o a valid autoboot.
        if (no_menu) {
            boot.mode = 1;
            smc_set_notification_led(LEDRAW_RED);
            goto skip_menu;
        }

        printf("Showing menu...\n");
        menu_init(&menu_main);

        smc_get_events();
        smc_set_odd_power(true);
    }
#endif // !FASTBOOT

skip_menu:
#ifdef MEASURE_TIME
    u32 deinit_start = read32(LT_TIMER);
#endif

    if(!no_gpu)
        gpu_cleanup();

    printf("Unmounting SLC...\n");
    isfs_fini();

#ifndef FASTBOOT
    printf("Shutting down MLC...\n");
    mlc_exit();
    
    printf("Shutting down SD card...\n");
    ELM_Unmount();
    sdcard_exit();
#endif //!FASTBOOT

    printf("Shutting down interrupts...\n");
    irq_shutdown();

    printf("Shutting down caches and MMU...\n");
    mem_shutdown();

    switch(boot.mode) {
        case 0:
            if (boot.vector && main_force_pause) {
                printf("IOS is loaded and ready to launch!\n");
                printf("Swap SD card now...\n");
                console_power_to_continue();
            }

            if(boot.vector) {
#ifdef FASTBOOT
                prsh_set_entry("minute_boot", (void*)1, 0);
#else
                prsh_set_entry("minute_boot", (void*)(menu_main.selected + 1), 0);
#endif
                printf("Vectoring to 0x%08lX...\n", boot.vector);
            } else {
                printf("No vector address, hanging!\n");
                panic(0);
            }
            break;
        case 1: smc_power_off(); break;
        case 2: smc_reset(); break;
        case 3: smc_reset_no_defuse(); break;
    }

#ifdef MEASURE_TIME
    u32 end = read32(LT_TIMER);
    printf( "minute:        %u\n"
            " init:         %u\n"
            "  pre graphic  %u\n"
            "  graphic      %u\n"
            "  graph-> sd   %u\n"
            "  sd           %u\n"
            "  sd -> ini    %u\n"
            "  ini          %u\n"
            "  ini -> end   %u\n"
            " loading:      %u\n"
            " deinit        %u\n",
            end-minute_start_time,
            init_end-minute_start_time, graphic_start-minute_start_time, graphic_end-graphic_start, 
            sd_start-graphic_end, sd_end-sd_start, ini_start-sd_end, ini_end-ini_start, init_end-ini_end,
            deinit_start-init_end, end-deinit_start);
#endif // MEASURE_TIME

    printf("Jumping to IOS... GO GO GO\n");

    // WiiU-Firmware-Emulator JIT bug
    void (*boot_vector)(void) = (void*)boot.vector;
    boot_vector();

    return boot.vector;
}

int boot_ini(const char* key, const char* value)
{
    if(!strcmp(key, "autoboot") && !autoboot){
        if(!autoboot) // don't overwrite if already set by prsh
            autoboot = (u32)minini_get_uint(value, 0);
    } else if(!strcmp(key, "autoboot_file"))
        strncpy(autoboot_file, value, sizeof(autoboot_file));
    else if(!strcmp(key, "autoboot_timeout"))
        autoboot_timeout_s = (u32)minini_get_uint(value, 3);
    else if(!strcmp(key, "force_pause"))
        main_force_pause = minini_get_bool(value, 0);
    else if(!strcmp(key, "allow_legacy_patches"))
        main_allow_legacy_patches = minini_get_bool(value, 0);

    return 0;
}

int main_autoboot(void)
{
    if(autoboot >= menu_main.entries || autoboot < 1){
        printf("Invalid autoboot option: %i\n", autoboot);
        return -1;
    }
    menu_main.selected = autoboot-1;
    menu_item entry = menu_main.option[menu_main.selected];
    printf("Autobooting %i: %s\n", autoboot, entry.text);
    entry.callback();
    return 0;
}

void main_reload(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("fw.img");
    boot.needs_otp = 0;
    boot.is_patched = 0;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load fw.img!\n");
        console_power_to_continue();
    }
}

void main_shutdown(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 1;
    menu_reset();
}

void main_reset(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 2;
    menu_reset();
}

void main_reset_no_defuse(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.mode = 3;
    menu_reset();
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
    console_power_to_exit();
}


void main_quickboot_patch_slc(void)
{
    gfx_clear(GFX_ALL, BLACK);
    if(isfs_init(ISFSVOL_SLC)<0){
        console_power_to_continue();
        return;
    }
    boot.vector = ancast_patch_load("slc:/sys/title/00050010/1000400a/code/fw.img", "ios.patch", slc_plugin_dir, false); // ios_orig.img
    boot.is_patched = 1;
    boot.needs_otp = 1;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load IOS with patches!\n");
        console_power_to_continue();
    }
}

void main_quickboot_isfshax(void){
    gfx_clear(GFX_ALL, BLACK);
    if(isfs_init(ISFSVOL_SLC)<0){
        console_power_to_continue();
        return;
    }

    boot.vector = boot1_patch_isfshax();
    if(!boot.vector){
        printf("Failed to load IOS with ISFShax patch\n");
        console_power_to_continue();
        return;
    }

    boot.mode = 0;
    menu_reset();
}


void main_quickboot_patch(void)
{
    gfx_clear(GFX_ALL, BLACK);
    if(isfs_init(ISFSVOL_SLC)<0){
        console_power_to_continue();
        return;
    }
    boot.vector = ancast_patch_load("slc:/sys/title/00050010/1000400a/code/fw.img", "ios.patch", sd_plugin_dir, false); // ios_orig.img
    boot.is_patched = 1;
    boot.needs_otp = 1;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load IOS with patches!\n");
        console_power_to_continue();
    }
}

void main_swapboot_patch(void)
{
    gfx_clear(GFX_ALL, BLACK);
    boot.vector = ancast_patch_load("ios_orig.img", "ios_orig.patch", sd_plugin_dir, false);
    boot.is_patched = 1;
    boot.needs_otp = 1;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load IOS with patches!\n");
        console_power_to_continue();
    }
}

void main_quickboot_patch_rednand(void)
{
    gfx_clear(GFX_ALL, BLACK);
    int error = init_rednand();
    if(error<0){
        console_power_to_continue();
        return;
    }
    if(error){
        printf("Continue\n");
        if (console_abort_confirmation_power_no_eject_yes()){
            clear_rednand();
            return;
        }
    }
    if(rednand.slc.lba_length){
        if(isfs_init(ISFSVOL_REDSLC)<0){
            console_power_to_continue();
            return;
        }
        boot.vector = ancast_patch_load("redslc:/sys/title/00050010/1000400a/code/fw.img", "ios.patch", sd_plugin_dir, true);
    } else {
        if(isfs_init(ISFSVOL_SLC)<0){
            console_power_to_continue();
            return;
        }
        boot.vector = ancast_patch_load("slc:/sys/title/00050010/1000400a/code/fw.img", "ios.patch", sd_plugin_dir, true);
    }
    boot.is_patched = 1;
    boot.needs_otp = 1;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load IOS with patches!\n");
        console_power_to_continue();
    }
}

void main_swapboot_patch_rednand(void)
{
    int error = init_rednand();
    if(error<0){
        console_power_to_continue();
        return;
    }
    if(error){
        printf("Continue\n");
        if (console_abort_confirmation_power_no_eject_yes()){
            clear_rednand();
            return;
        }
    }
    gfx_clear(GFX_ALL, BLACK);
    boot.vector = ancast_patch_load("ios_orig.img", "ios_orig.patch", sd_plugin_dir, true);
    boot.is_patched = 1;
    boot.needs_otp = 1;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load IOS with patches!\n");
        console_power_to_continue();
    }
}

void main_quickboot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    boot.vector = ancast_iop_load("ios.img");
    boot.needs_otp = 1;
    boot.is_patched = 0;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load 'ios.img'!\n");
        console_power_to_continue();
    }
}

void main_boot_fw(void)
{
    gfx_clear(GFX_ALL, BLACK);

    char path[_MAX_LFN] = {0};
    pick_file("sdmc:", false, path);

    boot.vector = ancast_iop_load(path);
    boot.needs_otp = 1;
    boot.is_patched = 0;

    if(boot.vector) {
        boot.mode = 0;
        menu_reset();
    } else {
        printf("Failed to load '%s'!\n", path);
        console_power_to_continue();
    }
}

void main_reset_crash(void)
{
	gfx_clear(GFX_ALL, BLACK);

	printf("Clearing SMC crash buffer...\n");

	const char buffer[64 + 1] = "Crash buffer empty.";
	rtc_set_panic_reason(buffer);

    console_power_to_exit();
}

void main_get_crash(void)
{
    gfx_clear(GFX_ALL, BLACK);
    printf("Reading SMC crash buffer...\n");

    char buffer[64 + 1] = {0};
    rtc_get_panic_reason(buffer);

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

    console_power_to_exit();
}

void main_credits(void)
{
    gfx_clear(GFX_ALL, BLACK);
    console_init();

    console_add_text("minute (not minute) - a Wii U port of mini\n");

    console_add_text("The SALT team: Dazzozo, WulfyStylez, shinyquagsire23 and Relys (in spirit)\n");

    console_add_text("Other Contributers: Jan\n");

    console_add_text("Special thanks to fail0verflow (formerly Team Twiizers) for the original \"mini\", and for the vast\nmajority of Wii research and early Wii U research!\n");

    console_add_text("Thanks to all WiiUBrew contributors, including: Hykem, Marionumber1, smea, yellows8, derrek,\nplutoo, naehrwert...\n");

    console_add_text("Press POWER to exit.");

    console_show();
    console_power_to_exit();
}

void main_interactive_console(void)
{
    intcon_init();
    intcon_show();
}

void silly_tests()
{
#if 0
    //00c9401b read32(0x0d8001e4)
    while (1) {
        char serial_input[256];
        //smc_set_cc_indicator(LED_ON);
        u8 input = smc_get_events();

        serial_poll();
        int amt = serial_in_read(serial_input);
        if (amt) {
            printf("%s", serial_input);
        }

        
        //printf("\n%02x %02x\n", test_read_serial, input);
        //udelay(1000*1000);
    }
#endif

    //printf("%x\n", read32(LT_SYSCFG1));
    
#if 0
    set32(LT_MEMIRR, 1<<6);

    printf("%08x\n", read32(LT_DBGBUSRD));
    
    while (1)
    {
#if 0
        for (int j = 0; j < 0x10000; j++)
        {
            for (int i = 0; i < 0x2; i++)
            {
                write32(LT_DBGPORT, (0<<9) | (i<<15) | 0 | (0<<13) | (0<<12) | (j<<16)); //| (1<<25) // 
                printf("%02x %04x: %08x\n", i, j, read32(LT_DBGBUSRD));
            }
        }
#endif
        

        //write32(LT_DBGPORT, (1<<9) | (22<<1) | 0 | (0<<13) | (1<<12) | (22<<14)); //| (1<<25)
        //printf("asdf\n");
        //printf("%08x\n", read32(LT_DBGPORT));
        
        //write32(LT_DBGBUSRD, 0);

        //printf("%x\n", read32(LT_SYSCFG1));

        for (int i = 0; i < 32; i++)
        {
            //gpio2_basic_set(i, 0);
        }
        printf("asdf\n");
        udelay(1000*1000);
        for (int i = 0; i < 32; i++)
        {
            //gpio2_basic_set(i, 1);
        }
        udelay(1000*1000);
    }
#endif
}
#endif // !MINUTE_BOOT1
