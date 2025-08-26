#ifndef MKDISK_EXT2_H
#define MKDISK_EXT2_H

#include <common/common.h>

STRUCT(ext2_meta) {
    unsigned type_and_perm;
    unsigned uid, gid;
};

void ext2_init(void);
dir* ext2_root_directory(void);
dir* ext2_create_directory(dir* parent, const char* name, const ext2_meta* meta);
void ext2_add_file(dir* parent, const char* name, const void* data, size_t size, const ext2_meta* meta);
void ext2_write(void);

#endif
