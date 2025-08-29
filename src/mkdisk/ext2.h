#ifndef MKDISK_EXT2_H
#define MKDISK_EXT2_H

#include <mkdisk/mkdisk.h>

STRUCT(ext2_meta) {
    unsigned type_and_perm;
    unsigned uid, gid;
};

Ext2* Ext2_Init(Disk* dsk, FSDir** outRoot);
FSDir* Ext2_CreateDirectory(Ext2* e2, FSDir* parent, const char* name, const ext2_meta* meta);
void Ext2_AddFile(Ext2* e2, FSDir* parent, const char* name, const void* data, size_t size, const ext2_meta* meta);
void Ext2_Write(Ext2* e2);

#endif
