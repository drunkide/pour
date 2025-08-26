#ifndef MKDISK_MBR_DEFS_H
#define MKDISK_MBR_DEFS_H

#include <common/common.h>

#pragma pack(push, 1)

#define MBR_MARK 0xAA55
#define MBR_CODE_SIZE 440

#define ACTIVE 0x80

STRUCT(mbr_entry) {
    uint8_t attrib;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t lba_size;
};

STRUCT(mbr) {
    uint8_t code[MBR_CODE_SIZE];
    uint32_t diskID;
    uint16_t reserved;
    mbr_entry entries[4];
    uint16_t mark;
};

#pragma pack(pop)

#endif
