#ifndef MKDISK_FAT_H
#define MKDISK_FAT_H

#include <common/common.h>

extern bool fat_enable_lfn;

void fat_init(const uint8_t* bootCode);
dir* fat_root_directory(void);
void fat_normalize_name(char* dst, const char* name);
dir* fat_create_directory(dir* parent, const char* name);
void fat_add_file(dir* parent, const char* name, const void* data, size_t size);
void fat_write(void);

#endif
