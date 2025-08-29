#ifndef MKDISK_FAT_H
#define MKDISK_FAT_H

#include <mkdisk/mkdisk.h>

Fat* Fat_Init(Disk* dsk, const uint8_t* bootCode, FSDir** outRoot);
void fat_normalize_name(char* dst, const char* name);
FSDir* fat_create_directory(FSDir* parent, const char* name);
void fat_add_file(FSDir* parent, const char* name, const void* data, size_t size);
void Fat_Write(Disk* dsk);

#endif
