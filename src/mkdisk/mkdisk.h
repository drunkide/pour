#ifndef MKDISK_MKDISK_H
#define MKDISK_MKDISK_H

#include <common/common.h>

STRUCT(dir);

STRUCT(lua_dir) {
    const char* path;
    dir* dir;
};

extern bool g_use_ext2;
extern bool g_mbr_fat;

lua_dir* get_directory(lua_State* L, int index);

#endif
