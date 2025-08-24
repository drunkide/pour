#include <common/dirs.h>
#include <common/script.h>
#include <common/utf8.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static char g_rootDir_[DIR_MAX];
static char g_toolsDir_[DIR_MAX];
static char g_dataDir_[DIR_MAX];
static char g_packagesDir_[DIR_MAX];

const char* const g_rootDir = g_rootDir_;
const char* const g_toolsDir = g_toolsDir_;
const char* const g_dataDir = g_dataDir_;
const char* const g_packagesDir = g_packagesDir_;

char* Dir_FindLastSeparator(const char* ptr)
{
    char* p = strrchr(ptr, '/');
  #ifdef _WIN32
    char* p2 = strrchr(ptr, '\\');
    if (p2 && (!p || p2 > p))
        p = p2;
  #endif
    return p;
}

bool Dir_IsSeparator(char ch)
{
    if (ch == '/')
        return true;
  #ifdef _WIN32
    if (ch == '\\')
        return true;
  #endif
    return false;
}

#ifdef _WIN32
static bool Dir_IsDriveLetter(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
#endif

bool Dir_IsAbsolutePath(const char* path)
{
    if (Dir_IsSeparator(*path))
        return true;

  #ifdef _WIN32
    if (Dir_IsDriveLetter(*path) && path[1] == ':')
        return true;
  #endif

    return false;
}

bool Dir_IsRoot(const char* path)
{
    if (Dir_IsSeparator(*path)) {
        const char* p = path + 1;
        while (Dir_IsSeparator(*p))
            ++p;
        return (*p == 0);
    }

  #ifdef _WIN32
    if (Dir_IsDriveLetter(*path) && path[1] == ':') {
        const char* p = path + 2;
        if (!*p)
            return true;
        while (Dir_IsSeparator(*p))
            ++p;
        return (*p == 0);
    }
  #endif

    return false;
}

void Dir_RemoveTrailingPathSeparator(char* path)
{
    size_t len = strlen(path);
    while (len > 0 && Dir_IsSeparator(path[len - 1]))
        path[--len] = 0;
}

static void Dir_RemoveExtraTrailingPathSeparators(char* path)
{
    size_t len = strlen(path);
    if (len > 0 && Dir_IsSeparator(path[len - 1])) {
        while (len > 1 && Dir_IsSeparator(path[len - 2]))
            path[--len] = 0;
    }
}

bool Dir_RemoveLastPath(char* path)
{
    if (Dir_IsRoot(path)) {
        Dir_RemoveExtraTrailingPathSeparators(path);
        return false;
    }

    Dir_RemoveTrailingPathSeparator(path);

    char* p = Dir_FindLastSeparator(path);
    if (!p)
        return false;

    *p = 0;
    return true;
}

void Dir_EnsureTrailingPathSeparator(char* path)
{
    size_t len = strlen(path);
    if (len > 0 && !Dir_IsSeparator(path[len - 1])) {
        path[len] = '/';
        path[len + 1] = 0;
    }
}

void Dir_MakeAbsolutePath(char* path)
{
    lua_State* L = gL;

    if (Dir_IsAbsolutePath(path))
        return;

  #ifdef _WIN32

    WCHAR cwd[MAX_PATH];
    cwd[0] = 0;
    GetCurrentDirectoryW(MAX_PATH, cwd);

    Utf8_PushConvertFromUtf16(L, cwd);
    lua_pushliteral(L, "\\");

  #else

    char cwd[PATH_MAX] = {0};
    getcwd(cwd, sizeof(cwd));

    lua_pushstring(L, cwd);
    lua_pushliteral(L, "/");

  #endif

    lua_pushstring(L, path);
    lua_concat(L, 3);

    size_t len;
    const char* src = lua_tolstring(L, -1, &len);
    memcpy(path, src, len + 1);

    lua_pop(L, 1);
}

void Dir_FromNativeSeparators(char* path)
{
  #ifdef _WIN32
    for (char* p = path; ; ++p) {
        if (*p == '\\')
            *p = '/';
        else if (*p == 0)
            break;
    }
  #endif
}

void Dir_ToNativeSeparators(char* path)
{
  #ifdef _WIN32
    for (char* p = path; ; ++p) {
        if (*p == '/')
            *p = '\\';
        else if (*p == 0)
            break;
    }
  #endif
}

void Dir_AppendPath(char* path, const char* element)
{
    Dir_EnsureTrailingPathSeparator(path);
    strcat(path, element);
}

void Dirs_Init(void)
{
  #ifdef POUR_ROOT_DIR
    strcpy(g_rootDir_, POUR_ROOT_DIR);
  #else
    strcpy(g_rootDir_, __FILE__);
    Dir_RemoveLastPath(g_rootDir_); /* remove file name */
    Dir_RemoveLastPath(g_rootDir_); /* remove 'common' */
  #endif
    Dir_RemoveLastPath(g_rootDir_); /* remove 'src' */
    Dir_FromNativeSeparators(g_rootDir_);

    strcpy(g_toolsDir_, g_rootDir_);
    Dir_AppendPath(g_toolsDir_, "tools");

    strcpy(g_dataDir_, g_rootDir_);
    Dir_AppendPath(g_dataDir_, "data");

    strcpy(g_packagesDir_, g_dataDir_);
    Dir_AppendPath(g_packagesDir_, "packages");
}
