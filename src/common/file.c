#include <common/file.h>
#include <common/script.h>
#include <common/utf8.h>
#include <common/dirs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <malloc.h>

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
    if (!result && GetLastError() == ERROR_PATH_NOT_FOUND) {
        size_t pathLen = strlen(path) + 1;
        char* buf = (char*)alloca(pathLen);
        memcpy(buf, path, pathLen);
        if (Dir_RemoveLastPath(buf) && File_CreateDirectory(buf))
            result = CreateDirectoryW(wpath, NULL);
    }
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

bool File_Write(const char* file, const void* data, size_t size)
{
  #ifdef _WIN32

    const WCHAR* wfile = (const WCHAR*)Utf8_PushConvertToUtf16(gL, file, NULL);

    HANDLE hFile = CreateFileW(wfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD dwBytesWritten;
    if (!WriteFile(hFile, data, (DWORD)size, &dwBytesWritten, NULL))
        return false;
    if (dwBytesWritten != size)
        return false;

    CloseHandle(hFile);
    return true;

  #else

    FILE* f = fopen(file, "wb");
    if (!f)
        return false;

    size_t bytesWritten = fwrite(data, 1, size, f);
    if (ferror(f) || bytesWritten != size)
        return false;

    fclose(f);
    return true;

  #endif
}
