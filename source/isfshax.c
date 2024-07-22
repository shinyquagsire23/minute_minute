/*
 * isfshax.c
 *
 * Copyright (C) 2021          rw-r-r-0644 <rwrr0644@gmail.com>
 *
 * This code is licensed to you under the terms of the GNU GPL, version 2;
 * see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 *
 *
 * isfshax is installed to 4 superblock slots for redundancy
 * against ecc errors and nand blocks wear out.
 *
 * when a boot1 superblock recommit attempt (due to ecc errors)
 * is detected, the superblock will be rewritten to the next
 * isfshax slot after correcting ecc errors.
 *
 * the generation number is incremented after each correction;
 * once the maximum allowed genertion number is reached, all
 * isfshax superblocks are rewritten with a lower generation number.
 *
 * if one of the blocks becomes bad during the rewrite,
 * the range of used generations numbers is incremented to ensure
 * the old superblock will not be used:
 * 0 BAD BLOCKS -> generation no. 0xfffffaff-0xfffffbff
 * ...
 * 3 BAD BLOCKS -> generation no. 0xfffffdff-0xfffffeff
 *
 * bad block information is stored along with other informations in a
 * separate isfshax info structure inside of the superblock, since all
 * isfshax slots are already marked as bad to guard against IOSU intervention
 * inside of the normal ISFS cluster allocation table.
 */
#include "types.h"
#include "isfs.h"
#include "isfshax.h"
#include "malloc.h"

#include <stdio.h>

#define ISFSVOL_SLC 0
#define SLC_SUPER_COUNT 64

#ifdef NAND_WRITE_ENABLED

/* boot1 superblock loading address.
 * this can be used to read the current generation
 * and the list of isfshax superblock slots, but it
 * can't be rewritten directly to nand as it has been
 * modified by the stage1 payload. */
static const isfshax_super *
boot1_superblock = (const isfshax_super *)(0x01f80000);

static isfshax_super superblock;


static bool isfshax_needs_rewrite(int res, bool slot_ecc_correctable){
    if(!res)
        return false;
    // don't rewrite if the slot is known for having a correctable error
    if(res == ISFSVOL_ECC_CORRECTED && slot_ecc_correctable)
        return false;
    return true;
}

static int isfshax_rewrite_super(isfs_ctx *slc, u32 index, u32 generation, isfshax_super *superblock){
    superblock->isfshax.generation = generation;
    superblock->generation = generation;
    superblock->isfshax.index = index;

    int res;
    for(int retrys=3; retrys; retrys--){
        isfshax_slot *slot = &superblock->isfshax.slots[index];
        res = isfs_write_super(slc, superblock, slot->slot);
        if(!res)
            return 0;
        if(res == ISFSVOL_ECC_CORRECTED){
            if(slot->ecc_correctable)
                return res;
            if(retrys<3){
                slot->ecc_correctable = true;
                retrys++;
            }
        }        
    }
    return res;
}

/**
 * @brief check superblocks and rewrite ones becomming bad
 * 
 * Never overwrites the current superblock
 * 
 * @return int 
 */

int isfshax_refresh(void)
{
    isfs_ctx *slc = isfs_get_volume(ISFSVOL_SLC);
    slc->version = 1;
    isfs_load_keys(slc);


    int num_rewrite = 0;
    bool needs_rewrite[ISFSHAX_REDUNDANCY];
    bool is_good[ISFSHAX_REDUNDANCY];
    int good_count = 0;
    bool good_no_rewrite = false;

    u32 curindex = boot1_superblock->isfshax.index;
    u8 curslot = boot1_superblock->isfshax.slots[curindex].slot;
    bool curecc = !!(boot1_superblock->isfshax.slots[curindex].ecc_correctable);

    
    u32 newest_gen = boot1_superblock->isfshax.generation;
    u32 newest_gen_index = curindex;
    u32 rewrite_index = (curindex + 1) % ISFSHAX_REDUNDANCY;
    //u32 rewrite_gen = max(ISFSHAX_GENERATION_FIRST, newest_gen-1)
    bool rewrite_needed = false;
    bool read_bad_slots[ISFSHAX_REDUNDANCY] = {};

    bool used_gens[ISFSHAX_REDUNDANCY-1] = {};

    int bad_slot_count = 0;

    for(int i=1; i<ISFSHAX_REDUNDANCY; i++){
        u32 index = (curindex + i) % ISFSHAX_REDUNDANCY;
        if(boot1_superblock->isfshax.slots[index].bad){
            bad_slot_count++;
            continue;        
        }
        u32 slot = boot1_superblock->isfshax.slots[index].slot;
        int res = isfs_read_super(slc, &superblock, slot);
        if(!rewrite_needed && isfshax_needs_rewrite(res, boot1_superblock->isfshax.slots[index].ecc_correctable)){
            rewrite_index = index;
            rewrite_needed = true;                
        } else if(res>=0) {
            int gen_idx = newest_gen - superblock.isfshax.generation -1;
            if(gen_idx>=0 && gen_idx < ISFSHAX_REDUNDANCY)
                used_gens[gen_idx] = true;
        }
        if(res<0){
            // prioritize rewriting uncorrectable blocks over correctable
            rewrite_index = index;
            read_bad_slots[index] = true;
            continue;
        }

        if(newest_gen < superblock.isfshax.generation){
            newest_gen = superblock.isfshax.generation;
            newest_gen_index = index;
        }
    }

    if(newest_gen > boot1_superblock->isfshax.generation)
        return ISFSHAX_ERROR_CURRENT_GEN_NOT_LATEST;
    
    int res = isfs_read_super(slc, &superblock, curslot);
    if(res<0)
        return ISFSHAX_ERROR_CURRENT_SLOT_BAD;

    bool update_generation = false;
    if(isfshax_needs_rewrite(res, boot1_superblock->isfshax.slots[curindex].ecc_correctable)){
        update_generation = true;
        rewrite_needed = true;
    }

    if(!rewrite_needed){
        return bad_slot_count;
    }

    u32 write_gen = newest_gen + 1;
    if(!update_generation){
        int back = 0;
        while(used_gens[back--]);
        u32 free_gen = newest_gen - back;
        if(free_gen>=ISFSHAX_GENERATION_FIRST)
            write_gen=free_gen;
    }

    for(int i=0; i<ISFSHAX_REDUNDANCY; i++){
        if(rewrite_index==curindex)
            continue;
        if(write_gen > ISFSHAX_GENERATION_FIRST + ISFSHAX_GENERATION_RANGE)
            return ISFSHAX_ERROR_EXCEEDED_GENERATION;
        res = isfshax_rewrite_super(slc, rewrite_index, write_gen, &superblock);
        if(res>=0)
            return (i?ISFSHAX_REWRITE_SLOT_BECAME_BAD:ISFSHAX_REWRITE_HAPPENED) + bad_slot_count;

        superblock.isfshax.slots[rewrite_index].bad = true;
        write_gen = newest_gen + ISFSHAX_REDUNDANCY + i;
        if(!read_bad_slots[rewrite_index])
            bad_slot_count++;

        rewrite_index = (rewrite_index+1) % ISFSHAX_REDUNDANCY;
    }

    return ISFSHAX_ERROR_NO_REDUNDENCY;
}
#endif //NAND_WRITE_ENABLED


void print_isfshax_refresh_error(int isfshax_refresh){
    if(!isfshax_refresh)
        return;
    if(isfshax_refresh<0){
        printf("\n\nCRITICAL!!!\n");
        switch (isfshax_refresh)
        {
        case ISFSHAX_ERROR_NO_REDUNDENCY:
                printf("All ISFShax backup superblocks became bad!\n");
                printf("Install ISFShax again ASAP!\n");
                break;
        case ISFSHAX_ERROR_CURRENT_GEN_NOT_LATEST:
                printf("Boot1 rejected last superblock, but it seems good.\n");
                printf("This shouldn't happen, report it as a bug\n");
                break;
        case ISFSHAX_ERROR_CURRENT_SLOT_BAD:
                printf("Current ISFShax superblock failed check!\n");
                printf("This shouldn't happen, report it as a bug\n");
                break;
        case ISFSHAX_ERROR_EXCEEDED_GENERATION:
                printf("ISFShax generation range exeeded\n");
                printf("This shouldn't happen, report it as a bug\n");
                break;
        default:
            printf("Unknown ISFShax refresh error: %d\n", isfshax_refresh);
        }
    } else {
        int badblocks = isfshax_refresh & 0xF;
        int event = isfshax_refresh & ~0xF;
        if(badblocks)
            printf("WARNING: %d ISFShax superblock are bad\n");
        switch (event)
        {
        case ISFSHAX_REWRITE_HAPPENED:
            printf("WARNING: A ISFShax superblock was successfully rewritten\n");
            break;
        case ISFSHAX_REWRITE_SLOT_BECAME_BAD:
            printf("WARNING: ISFShax superblock failed during refresh!\n");
            break;
        case 0:
        default:
        }
    }

    printf("ISFShax refresh reported: %d\n", isfshax_refresh);
}
