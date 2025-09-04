#ifndef COMMON_DIRS_H
#define COMMON_DIRS_H

#include <common/common.h>

#ifdef _WIN32
#define DIR_SEPARATOR "\\"
#else
#define DIR_SEPARATOR "/"
#endif

#define DIR_MAX 1024

extern const char* const g_rootDir;
extern const char* const g_installDir;
extern const char* const g_dataDir;
extern const char* const g_packagesDir;
extern const char* const g_targetsDir;

char* Dir_FindLastSeparator(const char* ptr);

bool Dir_IsSeparator(char ch);
bool Dir_IsAbsolutePath(const char* path);
bool Dir_IsRoot(const char* path);
void Dir_RemoveTrailingPathSeparator(char* path);
bool Dir_RemoveLastPath(char* path);
void Dir_EnsureTrailingPathSeparator(char* path);

const char* Dir_PushAbsolutePath(lua_State* L, const char* path);
void Dir_MakeAbsolutePath(lua_State* L, char* path, size_t pathMax);

void Dir_FromNativeSeparators(char* path);
void Dir_ToNativeSeparators(char* path);

void Dir_AppendPath(char* path, const char* element);

void Dirs_Init(lua_State* L);

#endif
