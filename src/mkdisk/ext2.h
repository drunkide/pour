#ifndef MKDISK_EXT2_H
#define MKDISK_EXT2_H

#include <mkdisk/mkdisk.h>

STRUCT(ext2_meta) {
    unsigned type_and_perm;
    unsigned uid, gid;
};

void Ext2_Init(Disk* dsk, FSDir** outRoot);
FSDir* ext2_create_directory(FSDir* parent, const char* name, const ext2_meta* meta);
void ext2_add_file(FSDir* parent, const char* name, const void* data, size_t size, const ext2_meta* meta);
void Ext2_Write(Disk* dsk);

#endif
