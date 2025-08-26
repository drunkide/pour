#ifndef MKDISK_WRITE_H
#define MKDISK_WRITE_H

#include <common/common.h>

void write_begin(const char* file, size_t fileSize);
void write_append(const void* data, size_t size);
size_t write_get_current_offset(void);
void write_commit(void);

#endif
