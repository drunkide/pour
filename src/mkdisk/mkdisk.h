#ifndef MKDISK_MKDISK_H
#define MKDISK_MKDISK_H

#include <common/common.h>

typedef enum fs_t {
    FS_FAT,
    FS_EXT2,
} fs_t;

struct disk_config_t;

STRUCT(Ext2);
STRUCT(Fat);
STRUCT(FSDir);

STRUCT(Disk) {
    Disk* prev;
    Disk* next;
    lua_State* L;
    const struct disk_config_t* config;
    const char* name;
    const char* outFile;
    Fat* fat;
    Ext2* ext2;
    lua_Integer ref;
    fs_t fs;
    bool mbrFAT;
    bool fatEnableLFN;
    bool inList;
    bool built;
};

STRUCT(DiskDir) {
    Disk* disk;
    const char* path;
    FSDir* dir;
};

DiskDir* MkDisk_GetDirectory(lua_State* L, int index);

void MkDisk_AddFileContent(Disk* dsk, const DiskDir* dstDir,
    const char* name, const char* data, size_t dataLen);

void MkDisk_InitLua(lua_State* L);
void MkDisk_WriteAllDisks(lua_State* L);

#endif
