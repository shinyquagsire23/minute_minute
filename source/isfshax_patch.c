#include <stdio.h>
#include <string.h>
#include "serial.h"

//14MiB
#define IOSU_SIZE (14*1024*1024)

#ifdef MINUTE_BOOT1
#   define  BOOT1_serial(data) serial_send_u32(data)
#else
#   define  BOOT1_serial(data)
#endif

bool isfshax_search_patch(size_t from, size_t to, const void *search, size_t search_len, int patch_off, const void* patch, size_t patch_len){
    bool ret = false;
    for(size_t p = from; p<=to-search_len; p+=4){
        if(!memcmp((void*)p, search, search_len)){
            printf("Applying patch %p at %p\n", patch, p + patch_off);
            BOOT1_serial(0x8D4FF00);
            memcpy((void*)p + patch_off, patch, patch_len);
            ret = true;
            return true;
        }
    }
    return ret;
}

bool isfshax_patch_apply(u32 fw_img_start){
    size_t end = fw_img_start + IOSU_SIZE;

    // should start at 0x10722718
    static const u8 isfshax_patch_pattern[] = { 0x0a, 0x00, 0x01, 0x03, 0xe2, 0x03, 0x30, 0x44, 0xe3, 0x53, 0x00, 0x44, 
                                                0x0a, 0x00, 0x00, 0xe0, 0xe3, 0xa0, 0x10, 0x00, 0xe3, 0xe0, 0x50, 0x00, 
                                                0xe5, 0x8d, 0x10, 0x14, 0xea, 0x00, 0x00, 0x3c };
    // limit max ISFS generation to 0x8000 to ignore the ISFShax superblock
    static const u8 isfshax_patch[] = { 0xe3, 0xe0, 0x59, 0x02 }; // mvn r5, #0x8000

    bool success = isfshax_search_patch(fw_img_start, end, isfshax_patch_pattern, sizeof(isfshax_patch_pattern), 0x1072272C-0x10722718, isfshax_patch, sizeof(isfshax_patch));
    BOOT1_serial(0x8D4D100 + success);
    if(!success)
        return false;

    // block updates
    static const char system_update_url[] = "https://nus.wup.shop.nintendo.net/nus/services/NetUpdateSOAP";
    static const u32 n = 0;
    success = isfshax_search_patch(fw_img_start, end, system_update_url, sizeof(system_update_url), 0, &n, sizeof(n));
    BOOT1_serial(0x8D4D200 + success);

    static const char system_update_command[] = "GetSystemUpdate";
    success = isfshax_search_patch(fw_img_start, end, system_update_command, sizeof(system_update_command), 0, &n, sizeof(n));
    BOOT1_serial(0x8D4D300 + success);

    static const u8 update_check_end[] = { 0xe2, 0x43, 0x30, 0x03, 0xe1, 0x50, 0x00, 0x03, 0x05, 0x9f, 0x00, 0xf0, 0x01, 0x2f, 0xff, 0x1e, 0xe2, 0x83, 0x30, 0x02, 0xe1, 0x50, 0x00, 0x03, 0x0a, 0x00, 0x00, 0x1e };
    static const u32 movr00 = 0xe3a00000;
    success = isfshax_search_patch(fw_img_start, end, update_check_end, sizeof(update_check_end), 8, &movr00, sizeof(movr00));
    BOOT1_serial(0x8D4D400 + success);


    // just in case prevent SCFM from formatting the slc
    static const u8 scfmFormat[] = { 0xe5, 0x9f, 0x15, 0x40, 0xe5, 0x9f, 0x24, 0xc0, 0xe3, 0xa0, 0x30, 0x00, 0xe5, 0x98, 0x00, 0x00, 0xeb, 0x00, 0x18, 0x8f, 0xe5, 0x9f, 0xe5, 0x30, 0xe5, 0x9f, 0xc5, 0x30, 0xe5, 0x9f, 0x14, 0xe8, 0xe5, 0x9f, 0x23, 0xc4 };
    static const u32 illegal_instruction = 0xFFFFFFFF;
    success = isfshax_search_patch(fw_img_start, end, scfmFormat, sizeof(scfmFormat), 0x10, &illegal_instruction, sizeof(illegal_instruction));
    BOOT1_serial(0x8D4D500 + success);


    // don't use standby for restart
    static const u8 reboot_case[] = { 0x4c, 0x2a, 0x23, 0x80, 0x1c, 0x26, 0x36, 0xc8, 0x68, 0x32, 0x03, 0x1b, 0x42, 0x9a, 0xd1, 0x01 };
    static const u16 movr01 = 0x2001;
    success = isfshax_search_patch(fw_img_start, end, reboot_case, sizeof(reboot_case), -10, &movr01, sizeof(movr01));
    BOOT1_serial(0x8D46600 + success);
    printf("Reboot patch: %i\n", success);

    // do full reboot instead of IOSU reload (else patches wouldnÄt be applied)
    static const u8 ios_reload_branch[] = { 0xe0, 0x91, 0x4b, 0xcc, 0x42, 0x9a, 0xd1, 0x00, 0xe1, 0xea };
    static const u16 adds4 = 0x3204;
    success = isfshax_search_patch(fw_img_start, end, ios_reload_branch, sizeof(ios_reload_branch), 0, &adds4, sizeof(adds4));
    BOOT1_serial(0x8D46700 + success);
    printf("Reload patch: %i\n", success);

    // Shutdown properly instead of going to Standby Mode (DRAM on)
    static const u8 shutdown_stuff[] = { 0x23, 0x80, 0x68, 0x10, 0x02, 0x1b, 0x42, 0x98, 0xd0, 0x0a, 0x23, 0xa3, 0x00, 0x9b, 0x58, 0xe3, 0x2b, 0x00, 0xd0, 0x04, 0x68, 0xfa, 0x03, 0xd2, 0xd4, 0x01, 0x20, 0x04, 0xe0, 0x00 };
    success = isfshax_search_patch(fw_img_start, end, shutdown_stuff, sizeof(shutdown_stuff), 26, &movr01, sizeof(movr01));
    BOOT1_serial(0x8D46800 + success);
    printf("Reboot patch: %i\n", success);

    return true; // still boot even if the other patches fail
}