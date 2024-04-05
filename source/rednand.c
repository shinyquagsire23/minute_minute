#include "rednand_config.h"
#include "mbr.h"
#include "nand.h"
#include "sdmmc.h"
#include "sdcard.h"
#include "crypto.h"
#include "isfs.h"

#include "ff.h"
#include "ini.h"

#include <string.h>
#include <malloc.h>

#define REDSLC_MMC_BLOCKS ((NAND_MAX_PAGE * PAGE_SIZE) / SDMMC_DEFAULT_BLOCKLEN)

static const char rednand_ini_file[] = "sdmc:/minute/rednand.ini"; 

static const char ini_error[] = "ERROR in rednand.ini: ";

char *redotp_path = "sdmc:/redotp.bin";


rednand_config rednand = { 0 };

otp_t *redotp = NULL;

static struct {
    bool slc;
    bool slc_set;
    bool slccmpt;
    bool slccmpt_set;
    bool mlc;
    bool mlc_set;
    bool disable_scfm;
    bool allow_sys_scfm;
    bool scfm_on_slccmpt;
} rednand_ini = { 0 };

static int rednand_ini_handler(void* user, const char* section, const char* name, const char* value)
{
    bool bool_val = false;
    if(!strcmp("true", value))
        bool_val = true;
    else if(strcmp("false", value)){
        printf("%sInvalid value: %s, section: %s, key: %s\n", ini_error, value, section, name);
        return 0; // we only have boolean values
    }

    if(!strcmp("partitions", section)){
        if(!strcmp("slc", name)){
            rednand_ini.slc = bool_val;
            rednand_ini.slc_set = true;
            return 1;
        }
        if(!strcmp("slccmpt", name)){
            rednand_ini.slccmpt = bool_val;
            rednand_ini.slccmpt_set = true;
            return 1;
        }
        if(!strcmp("mlc", name)){
            rednand_ini.mlc = bool_val;
            rednand_ini.mlc_set = true;
            return 1;
        }
        printf("%sInvalid partition: %s\n", ini_error, name);
        return 0;
    }

    if(!strcmp("scfm", section)){
        if(!strcmp("disable", name)){
            rednand_ini.disable_scfm = bool_val;
            return 1;
        }
        if(!strcmp("on_slccmpt", name)){
            rednand_ini.scfm_on_slccmpt = bool_val;
            return 1;
        }
        if(!strcmp("allow_sys", name)){
            rednand_ini.allow_sys_scfm = bool_val;
            return 1;
        }
        printf("%sInvalid scfm option: %s\n", ini_error, name);
        return 0;
    }

    printf("%sInvalid section: %s\n", ini_error, section);

    return 0;
}

static int rednand_load_ini(void)
{
    memset(&rednand_ini, 0, sizeof(rednand_ini));
    FILE* file = fopen(rednand_ini_file, "r");
    if(!file) {
        printf("minini: Failed to open `%s`!\n", rednand_ini_file);
        return -1;
    }

    int res = ini_parse_file(file, rednand_ini_handler, NULL);

    if(res)
        printf("%sLine: %i\n", ini_error, res);

    fclose(file);

    return res;
}

static u32 rednand_check_legacy(mbr_sector *mbr){
    for(int i=2; i<4; i++)
        if(mbr->partition[i].type != 0xAE){
            return false;
        }

    if(LD_DWORD(mbr->partition[3].lba_length) != REDSLC_MMC_BLOCKS * 2){
        return false;
    }

    if(LD_DWORD(mbr->partition[2].lba_length) != 0x3A20000){
        return false;
    }
    
    return true;
}

static int mbr_to_rednand_partition(partition_entry *mbr_part, rednand_partition *red_part, char* part_name){
    if(red_part->lba_start || red_part->lba_length){
        printf("WARNING: Duplicate rednand partition %s\n", part_name);
        return 1;
    }
    red_part->lba_start = LD_DWORD(mbr_part->lba_start);
    red_part->lba_length = LD_DWORD(mbr_part->lba_length);
    return 0;
}

int rednand_load_mbr(void){
    mbr_sector mbr ALIGNED(32) = {0};
    int res = sdcard_read(0, 1, &mbr);
    if(res) {
        printf("Failed to read sd MBR (%d)!\n", res);
        return -1;
    }

    bool legacy = rednand_check_legacy(&mbr);
    int part_error = 0;
    if(legacy){
        printf("Detected legacy rednand layout\n");
        mbr_to_rednand_partition(&mbr.partition[2], &rednand.mlc, "redmlc");
        u32 slc_start = LD_DWORD(mbr.partition[3].lba_start);
        rednand.slc.lba_start = slc_start;
        rednand.slc.lba_length = REDSLC_MMC_BLOCKS;
        rednand.slccmpt.lba_start = slc_start + REDSLC_MMC_BLOCKS;
        rednand.slccmpt.lba_length = REDSLC_MMC_BLOCKS;
    } else {
        for(int i=1; i < MBR_MAX_PARTITIONS; i++){
            switch(mbr.partition[i].type){

                case MBR_PARTITION_TYPE_MLC_NOSCFM:
                    rednand.disable_scfm = true;
                case MBR_PARTITION_TYPE_MLC:
                    part_error|= mbr_to_rednand_partition(&mbr.partition[i], &rednand.mlc, "mlc");
                    break;
                case MBR_PARTITION_TYPE_SLC:
                    part_error|= mbr_to_rednand_partition(&mbr.partition[i], &rednand.slc, "slc");
                    break;
                case MBR_PARTITION_TYPE_SLCCMPT:
                    part_error|= mbr_to_rednand_partition(&mbr.partition[i], &rednand.slccmpt, "slccmpt");
            }
        }
    }

    return part_error;
}

static int check_apply_partition(bool enabled, bool set, rednand_partition *part, char *name){
    int ret = 0;
    if(!enabled){
        if(part->lba_length && !set){
            printf("WARNING: %s partition exists but is not configured in rednand.ini\n", name);
            ret = 1;
        }
        memset(part, 0, sizeof(rednand_partition));
    } else if(!part->lba_length){
        printf("ERROR: No %s partition found\n", name);
        ret = -1;
    }

    return ret;
}

static int apply_ini_config(void){
    if(!rednand_ini.disable_scfm && !rednand_ini.slc && !rednand_ini.allow_sys_scfm && rednand_ini.mlc && !rednand_ini.scfm_on_slccmpt){
        printf("%sUsing sys scfm with red mlc needs to be explicitly allowed\n", ini_error);
        return -1;
    }

    if(rednand_ini.slc && !rednand_ini.mlc && !rednand_ini.allow_sys_scfm){
        printf("%sUsing red slc with sys mlc needs to be explicitly allowed\n", ini_error);
        return -1;
    }

    if(rednand_ini.slc && !rednand_ini.mlc && rednand_ini.disable_scfm){
        printf("%sDisabeling scfm for sys nand is not allowed\n", ini_error);
        return -1;
    }

    if(rednand_ini.slc && !rednand_ini.mlc && rednand_ini.scfm_on_slccmpt){
        printf("%sMigrating scfm for sys nand is not allowed\n", ini_error);
        return -1;
    }

    int slcerror = check_apply_partition(rednand_ini.slc, rednand_ini.slc_set, &rednand.slc, "slc");
    int slccmpterror = check_apply_partition(rednand_ini.slccmpt, rednand_ini.slccmpt_set, &rednand.slccmpt, "slccmpt");
    int mlcerror = check_apply_partition(rednand_ini.mlc, rednand_ini.mlc_set, &rednand.mlc, "mlc");
    if(slcerror<0 || slccmpterror <0 || mlcerror <0){
        return -1;
    }
    int ret = slcerror | slccmpterror | mlcerror;

    if(rednand_ini.mlc && rednand_ini.disable_scfm != rednand.disable_scfm){
        printf("WARNING: rednand.ini scfm config missmatches red mlc partition type\nContinuing can lead to CORRUPTION!!! Stop if you didn't expect this warning\n");
        rednand.disable_scfm = rednand_ini.disable_scfm;
        ret |= 2;
    }

    rednand.scfm_on_slccmpt = rednand_ini.scfm_on_slccmpt;
    if(rednand.disable_scfm && rednand.scfm_on_slccmpt){
        printf("WARNING: disabeling scfm on slccmpt\n");
        ret |= 4;
    }

    if(redotp && !(rednand.slc.lba_length && rednand.slccmpt.lba_length && rednand.mlc.lba_length)){
        // if the slc or slccmpt gets mounted with the wrong key, it can be corrupted
        // TODO: add option to use key and IV from system OTP
        // TODO: add override option
        printf("redOTP requires all partitions to be redirected to prevent corruption!\n");
        return -1;
    }

    printf("Rednand Config:\n slccmpt: %i\n slc: %i\n mlc: %i\n disable scfm: %i\n", rednand.slccmpt.lba_length, rednand.slc.lba_length, rednand.mlc.lba_length, rednand.disable_scfm);

    return ret;
}

static int rednand_load_opt(void){
    if(redotp)
        free(redotp);
    printf("Trying to load %s... ", redotp_path);
    FILE* otp_file = fopen(redotp_path, "rb");
    if (!otp_file){
        printf("NOT FOUND!\n");
        return 0;
    }
    printf("FOUND!\n");
    redotp = memalign(32, sizeof(*redotp));
    if(!redotp){
        printf("Error allocating memory for red_otp\n");
        return -1;
    }
    int read = fread(redotp, sizeof(*redotp), 1, otp_file);
    fclose(otp_file);
    if(read != 1){
        printf("Error loading %s\n", redotp_path);
        free(redotp);
        redotp = NULL;
        return -2;
    }
    memcpy(redotp->seeprom_key, otp.seeprom_key, sizeof(otp.seeprom_key));
    return 1;
}

void clear_rednand(void){
    isfs_unmount(ISFSVOL_REDSLC);
    isfs_unmount(ISFSVOL_REDSLCCMPT);
    memset(&rednand, 0, sizeof(rednand));
    if(redotp)
        free(redotp);
}

int init_rednand(void){
    clear_rednand();

    int redotp_error = rednand_load_opt();
    if(redotp_error < 0){
        return -4;
    }

    int ini_error = rednand_load_ini();
    if(ini_error < 0)
        return -1;

    int mbr_error = rednand_load_mbr();
    if(mbr_error < 0)
        return -2;
    
    int apply_error = apply_ini_config();
    if(apply_error < 0)
        return -3;

    rednand.initilized = true;
    return mbr_error | ini_error | apply_error;
}