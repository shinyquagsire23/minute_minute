#pragma once

#include <types.h>

typedef struct {
    u8 bootable;
    u8 chs_start[3];
    u8 type;
    u8 chs_end[3];
    u32 lba_start;
    u32 lba_length;
} PACKED partition_entry;

_Static_assert(sizeof(partition_entry) == 16, "partition_entry size must be 16!");

typedef struct {
    u8 bootstrap[446];
    partition_entry partition[4];
    u16 boot_signature;
} PACKED mbr_sector;

_Static_assert(sizeof(mbr_sector) == 512, "mbr_sector size must be 512!");