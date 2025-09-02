#ifndef COMMON_FILE_H
#define COMMON_FILE_H

#include <common/common.h>

typedef enum openmode_t {
    FILE_OPEN_SEQUENTIAL_READ,
    FILE_OPEN_MODIFY,
    FILE_CREATE_OVERWRITE,
} openmode_t;

STRUCT(File);
STRUCT(Dir);

#define MAX_FILE_SIZE (0x5fffffff)

bool File_Exists(lua_State* L, const char* path);

bool File_TryCreateDirectory(lua_State* L, const char* path);

void File_PushCurrentDirectory(lua_State* L);
void File_SetCurrentDirectory(lua_State* L, const char* path);

bool File_TryDelete(lua_State* L, const char* path);

void File_QueryInfo(lua_State* L, const char* path, bool* outIsDir, uint64_t* outSize);

Dir* File_PushOpenDir(lua_State* L, const char* path);
const char* File_ReadDir(Dir* dir);
void File_CloseDir(Dir* dir);

File* File_PushOpen(lua_State* L, const char* path, openmode_t mode);
void File_Close(File* file);
void File_MakeSparse(File* file);
size_t File_GetSize(File* file);
bool File_TrySetSize(File* file, size_t newSize);
void File_SetPosition(File* file, size_t offset);
void File_Read(File* file, void* buf, size_t size);
void File_Write(File* file, const void* buf, size_t size);

char* File_PushContents(lua_State* L, const char* path, size_t* outSize);
const char* File_PushContentsAsString(lua_State* L, const char* path);
void File_Overwrite(lua_State* L, const char* path, const void* data, size_t size);
void File_OverwriteSparse(lua_State* L, const char* path, const void* data, size_t size);
bool File_MaybeOverwrite(lua_State* L, const char* path, const void* newData, size_t newSize);

#endif
