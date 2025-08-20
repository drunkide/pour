#ifndef COMMON_DIRS_H
#define COMMON_DIRS_H

#include <common/common.h>

#define DIR_MAX 1024

extern const char* const g_rootDir;
extern const char* const g_toolsDir;
extern const char* const g_dataDir;

char* Dir_FindLastSeparator(const char* ptr);

bool Dir_IsSeparator(char ch);
bool Dir_IsAbsolutePath(const char* path);
bool Dir_IsRoot(const char* path);
void Dir_RemoveTrailingPathSeparator(char* path);
bool Dir_RemoveLastPath(char* path);
void Dir_EnsureTrailingPathSeparator(char* path);

void Dir_FromNativeSeparators(char* path);

void Dir_AppendPath(char* path, const char* element);

void Dirs_Init(void);

#endif
