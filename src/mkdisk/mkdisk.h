#ifndef MKDISK_MKDISK_H
#define MKDISK_MKDISK_H

#include <common/common.h>

typedef enum fs_t {
    FS_FAT,
    FS_EXT2,
} fs_t;

struct disk_config_t;

STRUCT(FSDir);

STRUCT(Disk) {
    lua_State* L;
    const struct disk_config_t* config;
    fs_t fs;
    bool mbrFAT;
    bool fatEnableLFN;
    bool built;
};

void MkDisk_InitLua(lua_State* L);

#endif
