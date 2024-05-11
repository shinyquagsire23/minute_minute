#include <stdio.h>
#include <string.h>
#include "serial.h"

//14MiB
#define IOSU_SIZE (14*1024*1024)

bool isfshax_search_patch(size_t from, size_t to, const void *search, size_t search_len, int patch_off, const void* patch, size_t patch_len){
    bool ret = false;
    for(size_t p = from; p<=to-search_len; p+=4){
        if(!memcmp((void*)p, search, search_len)){
            printf("Applying patch %p at %p\n", patch, p + patch_off);
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
    static const u8 isfshax_patch[] = { 0xe3, 0xe0, 0x59, 0x02 }; // mvn r5, #0x8000

    bool success = isfshax_search_patch(fw_img_start, end, isfshax_patch_pattern, sizeof(isfshax_patch_pattern), 0x1072272C-0x10722718, isfshax_patch, sizeof(isfshax_patch));
    if(!success)
        return false;

    static const char system_update_url[] = "https://nus.wup.shop.nintendo.net/nus/services/NetUpdateSOAP";
    static const u32 n = 0;
    isfshax_search_patch(fw_img_start, end, system_update_url, sizeof(system_update_url), 0, &n, sizeof(n));

    static const char system_update_command[] = "GetSystemUpdate";
    isfshax_search_patch(fw_img_start, end, system_update_command, sizeof(system_update_command), 0, &n, sizeof(n));

    static const u8 scfmFormat[] = { 0xe9, 0x2d, 0x40, 0x30, 0xe3, 0xa0, 0x50, 0x00, 0xe2, 0x4d, 0xd0, 0x08, 0xe3, 0xa0, 0x40, 0x00, 0xe8, 0x8d, 0x00, 0x30, 0xeb, 0xff, 0xff, 0x93, 0xe2, 0x8d, 0xd0, 0x08, 0xe8, 0xbd, 0x80, 0x30 };
    static const u32 illegal_instruction = 0xFFFFFFFF;
    isfshax_search_patch(fw_img_start, end, scfmFormat, sizeof(scfmFormat), 0, &illegal_instruction, sizeof(illegal_instruction));


    return true; // still boot even if the other patches fail
}