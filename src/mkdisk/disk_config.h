#ifndef MKDISK_DISK_CONFIG_H
#define MKDISK_DISK_CONFIG_H

#include <common/common.h>

STRUCT(disk_config_t) {
    long long vhd_size;
    int vhd_bat_size;
    int vhd_next_offset;
    int vhd_cylinders;
    int vhd_heads;
    int vhd_sectors_per_track;
    int mbr_chs_end_0;
    int mbr_chs_end_1;
    int mbr_chs_end_2;
    long long mbr_disk_start;
    long long mbr_disk_size;
    int fat_sectors_per_cluster;
    int fat_head_count;
};

extern const disk_config_t disk_3M;
extern const disk_config_t disk_20M;
extern const disk_config_t disk_100M;
extern const disk_config_t disk_400M;
extern const disk_config_t disk_500M;
extern const disk_config_t disk_510M;
extern const disk_config_t disk_520M;
extern const disk_config_t disk_1G;

#endif
