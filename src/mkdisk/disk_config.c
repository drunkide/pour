#include <common/common.h>
#include <mkdisk/disk_config.h>

const disk_config_t disk_3M = {
    0x00300000, /* vhd_size */
    0x0002,     /* vhd_bat_size */
    4,          /* vhd_next_offset */
    0x005A,     /* vhd_cylinders */
    0x04,       /* vhd_heads */
    0x11,       /* vhd_sectors_per_track */
    0x01,       /* mbr_chs_end_0 */
    0x48,       /* mbr_chs_end_1 */
    0x7F,       /* mbr_chs_end_2 */
    0x00000008, /* mbr_disk_start */
    0x000017F8, /* mbr_disk_size */
    0x02,       /* fat_sectors_per_cluster */
    0x02,       /* fat_head_count */
};

const disk_config_t disk_20M = {
    0x01400000, /* vhd_size */
    0x000A,     /* vhd_bat_size */
    4,          /* vhd_next_offset */
    0x025A,     /* vhd_cylinders */
    0x04,       /* vhd_heads */
    0x11,       /* vhd_sectors_per_track */
    0x01,       /* mbr_chs_end_0 */
    0xA0,       /* mbr_chs_end_1 */
    0x7F,       /* mbr_chs_end_2 */
    0x00000020, /* mbr_disk_start */
    0x00009FE0, /* mbr_disk_size */
    0x01,       /* fat_sectors_per_cluster */
    0x02,       /* fat_head_count */
};

const disk_config_t disk_100M = {
    0x63EA000,  /* vhd_size */
    0x32,       /* vhd_bat_size */
    4,          /* vhd_next_offset */
    0x03EB,     /* vhd_cylinders */
    0x0C,       /* vhd_heads */
    0x11,       /* vhd_sectors_per_track */
    0x03,       /* mbr_chs_end_0 */
    0xFF,       /* mbr_chs_end_1 */
    0x2B,       /* mbr_chs_end_2 */
    0x3F,       /* mbr_disk_start */
    0x31F11,    /* mbr_disk_size */
    4,          /* fat_sectors_per_cluster */
    4,          /* fat_head_count */
};

const disk_config_t disk_400M = {
    0x18FA8000, /* vhd_size */
    0x00C8,     /* vhd_bat_size */
    5,          /* vhd_next_offset */
    0x032C,     /* vhd_cylinders */
    0x10,       /* vhd_heads */
    0x3F,       /* vhd_sectors_per_track */
    0x0F,       /* mbr_chs_end_0 */
    0xFF,       /* mbr_chs_end_1 */
    0x2B,       /* mbr_chs_end_2 */
    0x3F,       /* mbr_disk_start */
    0x000C7D01, /* mbr_disk_size */
    0x10,       /* fat_sectors_per_cluster */
    0x10,       /* fat_head_count */
};

const disk_config_t disk_500M = {
    0x1F392000, /* vhd_size */
    0x00FA,     /* vhd_bat_size */
    5,          /* vhd_next_offset */
    0x03F7,     /* vhd_cylinders */
    0x10,       /* vhd_heads */
    0x3F,       /* vhd_sectors_per_track */
    0x0F,       /* mbr_chs_end_0 */
    0xFF,       /* mbr_chs_end_1 */
    0xF6,       /* mbr_chs_end_2 */
    0x0000003F, /* mbr_disk_start */
    0x000F9C51, /* mbr_disk_size */
    0x10,       /* fat_sectors_per_cluster */
    0x10,       /* fat_head_count */
};

const disk_config_t disk_510M = {
    0x1FE00000, /* vhd_size */
    0x00FF,     /* vhd_bat_size */
    5,          /* vhd_next_offset */
    0x040C,     /* vhd_cylinders */
    0x10,       /* vhd_heads */
    0x3F,       /* vhd_sectors_per_track */
    0x1F,       /* mbr_chs_end_0 */
    0xE0,       /* mbr_chs_end_1 */
    0xFB,       /* mbr_chs_end_2 */
    0x00000020, /* mbr_disk_start */
    0x000FEFE0, /* mbr_disk_size */
    0x10,       /* fat_sectors_per_cluster */
    0x20,       /* fat_head_count */
};

const disk_config_t disk_520M = {
    0x207C0000, /* vhd_size */
    0x0104,     /* vhd_bat_size */
    6,          /* vhd_next_offset */
    0x0420,     /* vhd_cylinders */
    0x10,       /* vhd_heads */
    0x3F,       /* vhd_sectors_per_track */
    0x1F,       /* mbr_chs_end_0 */
    0xBF,       /* mbr_chs_end_1 */
    0x0F,       /* mbr_chs_end_2 */
    0x3F,       /* mbr_disk_start */
    0x00103DC1, /* mbr_disk_size */
    0x20,       /* fat_sectors_per_cluster */
    0x20,       /* fat_head_count */
};

const disk_config_t disk_1G = {
    0x3FFC0000, /* vhd_size */
    0x200,      /* vhd_bat_size */
    7,          /* vhd_next_offset */
    0x0820,     /* vhd_cylinders */
    0x10,       /* vhd_heads */
    0x3F,       /* vhd_sectors_per_track */
    0x3F,       /* mbr_chs_end_0 */
    0xBF,       /* mbr_chs_end_1 */
    0x07,       /* mbr_chs_end_2 */
    0x3F,       /* mbr_disk_start */
    0x1FFDC1,   /* mbr_disk_size */
    0x20,       /* fat_sectors_per_cluster */
    0x40,       /* fat_head_count */
};

const disk_config_t* disk_config = &disk_500M;
