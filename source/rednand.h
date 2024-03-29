#include "rednand_config.h"
#include "crypto.h"

extern rednand_config rednand;
extern otp_t *redotp;

int init_rednand(void);

void clear_rednand(void);

int rednand_load_mbr(void);