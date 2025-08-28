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

struct File
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)
    HANDLE handle;
  #else
    FILE* handle;
  #endif
    lua_State* L;
    char name[1]; /* should be the last field */
};

#define FILE_MT "File*"

/********************************************************************************************************************/

bool File_Exists(lua_State* L, const char* path)
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(L, path, NULL);
    bool result = GetFileAttributesW(wpath) != INVALID_FILE_ATTRIBUTES;
    lua_pop(L, 1);
    return result;

  #else

    struct stat st;
    return stat(path, &st) == 0;

  #endif
}

bool File_TryCreateDirectory(lua_State* L, const char* path)
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(L, path, NULL);
    bool result = CreateDirectoryW(wpath, NULL);
    if (!result && GetLastError() == ERROR_PATH_NOT_FOUND) {
        size_t pathLen = strlen(path) + 1;
        char* buf = (char*)lua_newuserdatauv(L, pathLen, 0);
        memcpy(buf, path, pathLen);
        if (Dir_RemoveLastPath(buf) && File_TryCreateDirectory(L, buf))
            result = CreateDirectoryW(wpath, NULL);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return result;

  #else

    return mkdir(path) == 0;

  #endif
}

void File_PushCurrentDirectory(lua_State* L)
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    WCHAR* p;

    WCHAR cwd[MAX_PATH];
    DWORD dwSize = GetCurrentDirectoryW(MAX_PATH, cwd);
    if (dwSize == 0) {
      failed:
        luaL_error(L, "GetCurrentDirectory() failed: %p", (void*)(size_t)GetLastError());
    }

    p = cwd;
    if (dwSize > sizeof(cwd) / sizeof(cwd[0])) {
        p = (WCHAR*)lua_newuserdatauv(L, dwSize * sizeof(WCHAR), 0);
        *p = 0;
        dwSize = GetCurrentDirectoryW(dwSize, p);
        if (dwSize == 0)
            goto failed;
    }

    Utf8_PushConvertFromUtf16(L, p);

    if (p != cwd)
        lua_remove(L, -2);

  #else

    char cwd[PATH_MAX] = {0};
    if (!getcwd(cwd, sizeof(cwd)))
        luaL_error(L, "getcwd() failed: %s", strerror(errno));

    lua_pushstring(L, cwd);

  #endif
}

void File_SetCurrentDirectory(lua_State* L, const char* path)
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(L, path, NULL);
    if (!SetCurrentDirectoryW(wpath))
        luaL_error(L, "SetCurrentDirectory() failed: %p", (void*)(size_t)GetLastError());
    lua_pop(L, 1);

  #else

    if (setcwd(path) < 0)
        luaL_error(L, "setcwd() failed: %s", strerror(errno));

  #endif
}

/********************************************************************************************************************/

static int lua_closefile(lua_State* L)
{
    File_Close((File*)lua_touserdata(L, 1));
    return 0;
}

File* File_PushOpen(lua_State* L, const char* path, openmode_t mode)
{
    size_t nameLen = strlen(path) + 1;
    File* file = (File*)lua_newuserdatauv(L, offsetof(File, name) + nameLen, 0);
    memcpy(file->name, path, nameLen);
    int n = lua_gettop(L);

    file->handle = NULL;
    file->L = L;

    if (luaL_newmetatable(L, FILE_MT)) {
        lua_pushcfunction(L, lua_closefile);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, n);

  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    DWORD dwDesiredAccess, dwCreationDisposition, dwFlags;
    const char* action;

    switch (mode) {
        case FILE_OPEN_SEQUENTIAL_READ:
            dwDesiredAccess = GENERIC_READ;
            dwCreationDisposition = OPEN_EXISTING;
            dwFlags = FILE_FLAG_SEQUENTIAL_SCAN;
            action = "open";
            break;
        case FILE_CREATE_OVERWRITE:
            dwDesiredAccess = GENERIC_WRITE;
            dwCreationDisposition = CREATE_ALWAYS;
            dwFlags = 0;
            action = "create";
            break;
        default:
            assert(false);
            luaL_error(L, "invalid file open mode.");
            return NULL;
    }

    const WCHAR* wpath = (const WCHAR*)Utf8_PushConvertToUtf16(L, path, NULL);

    file->handle = CreateFileW(wpath, dwDesiredAccess, FILE_SHARE_READ, NULL, dwCreationDisposition, dwFlags, NULL);
    if (file->handle == INVALID_HANDLE_VALUE)
        luaL_error(L, "unable to %s file \"%s\" (code %p)", action, file, (void*)(size_t)GetLastError());

  #else

    const char* action, *mode;

    switch (mode) {
        case FILE_OPEN_SEQUENTIAL_READ:
            mode = "rb";
            action = "open";
            break;
        case FILE_CREATE_OVERWRITE:
            mode = "wb";
            action = "create";
            break;
        default:
            assert(false);
            luaL_error(L, "invalid file open mode.");
            return NULL;
    }

    file->handle = fopen(path, mode);
    if (!file->handle)
        luaL_error(L, "unable to %s file \"%s\": %s", action, file, strerror(errno));

  #endif

    lua_settop(L, n);
    return file;
}

void File_Close(File* file)
{
  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    HANDLE handle = file->handle;
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        file->handle = INVALID_HANDLE_VALUE;
    }

  #else

    FILE* handle = file->handle;
    if (handle) {
        fclose(handle);
        file->handle = NULL;
    }

  #endif
}

size_t File_GetSize(File* file)
{
    lua_State* L = file->L;

  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    SetLastError(NO_ERROR);

    DWORD hiPart = 0;
    DWORD loPart = GetFileSize(file->handle, &hiPart);
    if (loPart == INVALID_FILE_SIZE) {
        DWORD dwError = GetLastError();
        if (dwError != NO_ERROR) {
            luaL_error(L, "unable to %s file \"%s\" (code %p)",
                "determine size of", file->name, (void*)(size_t)dwError);
        }
    }

    if (hiPart != 0 || loPart > MAX_FILE_SIZE)
        luaL_error(L, "file \"%s\" is too large.", file->name);

    return (size_t)loPart;

  #else

    FILE* handle = file->handle;
    fseek(handle, 0, SEEK_END);
    long size = ftell(handle);
    fseek(handle, 0, SEEK_SET);

    if (ferror(handle) || size < 0) {
        luaL_error(L, "unable to %s file \"%s\": %s",
            "determine size of", file->name, strerror(errno));
    }

    if (size > MAX_FILE_SIZE)
        luaL_error(L, "file \"%s\" is too large.", file->name);

    return (size_t)size;

  #endif
}

void File_Read(File* file, void* buf, size_t size)
{
    char* ptr = (char*)buf;
    lua_State* L = file->L;

  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    HANDLE handle = file->handle;

    while (size != 0) {
        DWORD dwBytesRead;
        if (!ReadFile(handle, ptr, (DWORD)size, &dwBytesRead, NULL)) {
            luaL_error(L, "unable to %s file \"%s\" (code %p)",
                "read", file->name, (void*)(size_t)GetLastError());
        }

        if (dwBytesRead == 0)
            luaL_error(L, "unexpected end of file \"%s\".", file->name);

        ptr += dwBytesRead;
        size -= dwBytesRead;
    }

  #else

    FILE* handle = file->handle;

    while (size != 0) {
        size_t bytesRead = fread(ptr, 1, size, handle);
        if (ferror(handle)) {
            luaL_error(L, "unable to %s file \"%s\": %s",
                "read", file->name, strerror(errno));
        }

        if (bytesRead == 0)
            luaL_error(L, "unexpected end of file \"%s\".", file->name);

        ptr += bytesRead;
        size -= bytesRead;
    }

  #endif
}

void File_Write(File* file, const void* buf, size_t size)
{
    const char* ptr = (const char*)buf;
    lua_State* L = file->L;

  #if defined(_WIN32) && !defined(USE_POSIX_IO)

    HANDLE handle = file->handle;

    while (size != 0) {
        DWORD dwBytesWritten;
        if (!WriteFile(handle, ptr, (DWORD)size, &dwBytesWritten, NULL)) {
            luaL_error(L, "unable to %s file \"%s\" (code %p)",
                "write", file->name, (void*)(size_t)GetLastError());
        }

        if (dwBytesWritten == 0)
            luaL_error(L, "incomplete write in file \"%s\".", file->name);

        ptr += dwBytesWritten;
        size -= dwBytesWritten;
    }

  #else

    FILE* handle = file->handle;

    while (size != 0) {
        size_t bytesWritten = fwrite(ptr, 1, size, handle);
        if (ferror(handle)) {
            luaL_error(L, "unable to %s file \"%s\": %s",
                "write", file->name, strerror(errno));
        }

        if (bytesWritten == 0)
            luaL_error(L, "incomplete write in file \"%s\".", file->name);

        ptr += bytesWritten;
        size -= bytesWritten;
    }

  #endif
}

/********************************************************************************************************************/

char* File_PushContents(lua_State* L, const char* path, size_t* outSize)
{
    File* file = File_PushOpen(L, path, FILE_OPEN_SEQUENTIAL_READ);

    size_t fileSize = File_GetSize(file);
    char* buf = (char*)lua_newuserdatauv(L, fileSize, 0);

    File_Read(file, buf, fileSize);

    File_Close(file);
    lua_remove(L, -2);

    if (outSize)
        *outSize = fileSize;

    return buf;
}

void File_Overwrite(lua_State* L, const char* path, const void* data, size_t size)
{
    File* file = File_PushOpen(L, path, FILE_CREATE_OVERWRITE);
    File_Write(file, data, size);
    File_Close(file);
    lua_pop(L, 1);
}

static int lua_isfileidentical(lua_State* L)
{
    const char* path = lua_tostring(L, 1);
    const char* newData = (const char*)lua_touserdata(L, 2);
    size_t newSize = (size_t)lua_tointeger(L, 3);

    File* file = File_PushOpen(L, path, FILE_OPEN_SEQUENTIAL_READ);

    size_t oldSize = File_GetSize(file);
    if (newSize != oldSize)
        return 0;

    char* oldData = (char*)lua_newuserdatauv(L, oldSize, 0);
    File_Read(file, oldData, oldSize);
    File_Close(file);

    lua_pushboolean(L, memcmp(oldData, newData, newSize) == 0);
    return 1;
}

bool File_MaybeOverwrite(lua_State* L, const char* path, const void* newData, size_t newSize)
{
    if (File_Exists(L, path)) {
        lua_pushcfunction(L, lua_isfileidentical);
        lua_pushstring(L, path);
        lua_pushlightuserdata(L, (void*)newData);
        lua_pushinteger(L, (lua_Integer)newSize);
        int err = lua_pcall(L, 3, 1, 0);
        bool fileIdentical = (err == LUA_OK ? lua_toboolean(L, -1) : false);
        lua_pop(L, 1);
        if (fileIdentical)
            return false;
    }

    File_Overwrite(L, path, newData, newSize);
    return true;
}
