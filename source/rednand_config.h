#include "types.h"

typedef struct {
    u32 lba_start;
    u32 lba_length;
} PACKED rednand_partition;

typedef struct {
    rednand_partition slc;
    rednand_partition slccmpt;
    rednand_partition mlc;
    bool disable_scfm;
    bool scfm_on_slccmpt;
    bool initilized;
    //v2
    bool slc_nocrypto; //not implemented yet
    bool slccmpt_nocrypto; //not implemented yet
    bool mlc_nocrypto;
} PACKED rednand_config;

