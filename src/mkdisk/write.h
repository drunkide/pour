#ifndef MKDISK_WRITE_H
#define MKDISK_WRITE_H

#include <common/common.h>

STRUCT(Write);

Write* Write_Begin(lua_State* L, const char* file, size_t fileSize);
void Write_Append(Write* wr, const void* data, size_t size);
size_t Write_GetCurrentOffset(Write* wr);
void Write_Commit(Write* wr);

#endif
