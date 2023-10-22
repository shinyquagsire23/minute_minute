#pragma once

#include <types.h>

typedef struct {
    u8 bootable;
    u8 chs_start[3];
    u8 type;
    u8 chs_end[3];
    u8 lba_start[4]; // little endian
    u8 lba_length[4]; // little endian
} PACKED partition_entry;

_Static_assert(sizeof(partition_entry) == 16, "partition_entry size must be 16!");

typedef struct {
    u8 bootstrap[446];
    partition_entry partition[4];
    u8 boot_signature[2];
} PACKED mbr_sector;

_Static_assert(sizeof(mbr_sector) == 512, "mbr_sector size must be 512!");