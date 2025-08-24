#ifndef COMMON_FILE_H
#define COMMON_FILE_H

#include <common/common.h>

bool File_Exists(const char* path);

bool File_CreateDirectory(const char* path);
bool File_SetCurrentDirectory(const char* path);

bool File_Write(const char* file, const void* data, size_t size);

#endif
