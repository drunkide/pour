#include <common/file.h>
#include <common/script.h>
#include <common/utf8.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

bool File_Exists(const char* path)
{
  #ifdef _WIN32
    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(gL, path, NULL);
    bool result = GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES;
    lua_pop(gL, 1);
    return result;
  #else
    struct stat st;
    return stat(path, &st) == 0;
  #endif
}

bool File_CreateDirectory(const char* path)
{
  #ifdef _WIN32
    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(gL, path, NULL);
    bool result = CreateDirectoryW(wpath, NULL);
    lua_pop(gL, 1);
    return result;
  #else
    return mkdir(path) == 0;
  #endif
}

bool File_SetCurrentDirectory(const char* path)
{
  #ifdef _WIN32
    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(gL, path, NULL);
    bool result = SetCurrentDirectoryW(wpath);
    lua_pop(gL, 1);
    return result;
  #else
    return setcwd(path) == 0;
  #endif
}
