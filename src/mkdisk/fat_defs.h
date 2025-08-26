#ifndef MKDISK_FAT_DEFS_H
#define MKDISK_FAT_DEFS_H

#include <common/common.h>

#define SECTOR_SIZE 512
#define MAX_ROOT_DIR_ENTRIES 512
#define SIGNATURE 0xAA55
#define BOOT_CODE_SIZE 0x1C0

#define ROOT_DIR_SIZE (MAX_ROOT_DIR_ENTRIES * sizeof(fat_direntry))
#define ROOT_DIR_SECTORS ((ROOT_DIR_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE)

#define ATTR_DIR 0x10
#define ATTR_ARCHIVE 0x20

#pragma pack(push, 1)

STRUCT(fat_bootsector) {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t fatCount;
    uint16_t maxRootDirEntries;
    uint16_t totalSectorsOld;
    uint8_t mediaDescriptor;
    uint16_t sectorsPerFat;
    uint16_t sectorsPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectors;
    uint32_t totalSectors;
    uint8_t physicalDriveNumber;
    uint8_t reserved1;
    uint8_t extendedBootSignature;
    uint8_t serial[4];
    char volumeLabel[11];
    char fileSystem[8];
    uint8_t code[BOOT_CODE_SIZE];
    uint16_t signature;
};

STRUCT(fat_direntry) {
    char name[8];
    char ext[3];
    uint8_t attrib;
    uint8_t reserved[10];
    uint32_t lastModified;
    uint16_t firstCluster;
    uint32_t size;
};

#pragma pack(pop)

#endif
