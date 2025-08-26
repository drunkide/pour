#ifndef MKDISK_VHD_DEFS_H
#define MKDISK_VHD_DEFS_H

#include <common/common.h>

#pragma pack(push, 1)

#define VHD_SECTORS_PER_BLOCK 4096
#define VHD_SECTOR_SIZE 512
#define VHD_BLOCK_SIZE (VHD_SECTORS_PER_BLOCK * VHD_SECTOR_SIZE)

#define BAT_ENTRIES_PER_SECTOR (VHD_SECTOR_SIZE / sizeof(uint32_t))

STRUCT(vhd_footer) {
    char cookie[8];
    uint32_t features;
    uint32_t version;
    uint64_t dataOffset;
    uint32_t timestamp;
    char creator[4];
    uint32_t creatorVersion;
    char creatorHostOS[4];
    uint64_t originalSize;
    uint64_t currentSize;
    uint16_t numCylinders;
    uint8_t numHeads;
    uint8_t sectorsPerTrack;
    uint32_t diskType;
    uint32_t checksum;
    uint8_t uniqueId[16];
    uint8_t savedState;
    uint8_t reserved[427];
};

STRUCT(vhd_dynhdr) {
    char cookie[8];
    uint64_t dataOffset;
    uint64_t tableOffset;
    uint32_t headerVersion;
    uint32_t maxTableEntries;
    uint32_t blockSize;
    uint32_t checksum;
    uint8_t parentUniqueId[16];
    uint32_t parentTimestamp;
    uint32_t reserved;
    uint8_t parentUnicodeName[512];
    uint8_t parentLocatorEntry1[24];
    uint8_t parentLocatorEntry2[24];
    uint8_t parentLocatorEntry3[24];
    uint8_t parentLocatorEntry4[24];
    uint8_t parentLocatorEntry5[24];
    uint8_t parentLocatorEntry6[24];
    uint8_t parentLocatorEntry7[24];
    uint8_t parentLocatorEntry8[24];
    uint8_t reserved2[256];
};

STRUCT(vhd_block) {
    uint8_t bitmap[VHD_SECTOR_SIZE];
    uint8_t data[VHD_BLOCK_SIZE];
};

STRUCT(vhd_blockchain) {
    struct vhd_blockchain* next;
    unsigned long long offset;
    vhd_block data;
};

#pragma pack(pop)

#endif
