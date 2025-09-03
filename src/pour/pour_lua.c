#include <pour/pour_lua.h>
#include <pour/pour.h>
#include <common/script.h>
#include <common/dirs.h>
#include <common/file.h>
#include <common/utf8.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

static int pour_chdir(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);

    if (!File_Exists(L, file))
        File_TryCreateDirectory(L, file);

    File_SetCurrentDirectory(L, file);

    return 1;
}

static int pour_file_exists(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);
    lua_pushboolean(L, File_Exists(L, file));
    return 1;
}

static int pour_file_read(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);
    File_PushContentsAsString(L, file);
    return 1;
}

static int pour_file_write(lua_State* L)
{
    size_t dataLen;
    const char* file = luaL_checkstring(L, 1);
    const char* data = luaL_checklstring(L, 2, &dataLen);
    File_MaybeOverwrite(L, file, data, dataLen);
    return 0;
}

static int pour_fetch(lua_State* L)
{
    const char* package = luaL_checkstring(L, 1);
    if (!Pour_Install(L, package, true))
        return luaL_error(L, "could not fetch required package '%s'.", package);
    return 0;
}

static int pour_require(lua_State* L)
{
    const char* package = luaL_checkstring(L, 1);
    if (!Pour_Install(L, package, false))
        return luaL_error(L, "could not install required package '%s'.", package);
    return 0;
}

static int pour_run(lua_State* L)
{
    int argc = lua_gettop(L);
    const char* package = luaL_checkstring(L, 1);

    char** argv = (char**)lua_newuserdatauv(L, argc * sizeof(char**), 0);
    argv[0] = (char*)package;
    for (int i = 1; i < argc; i++) {
        size_t argLen;
        const char* arg = luaL_checklstring(L, i + 1, &argLen);

        ++argLen;
        argv[i] = (char*)lua_newuserdatauv(L, argLen, 0);
        memcpy(argv[i], arg, argLen);
    }

    /*
    runmode_t mode;
    if (!strcmp(modestr, "wait"))
        mode = RUN_WAIT;
    else if (!strcmp(modestr, "nowait"))
        mode = RUN_DONT_WAIT;
    else if (!strcmp(modestr, "nowait,noconsole"))
        mode = RUN_DONT_WAIT_NO_CONSOLE;
    else
        return luaL_error(L, "invalid value \"%s\" for the wait argument.", modestr);
    */

    if (!Pour_Run(L, package, NULL, argc, argv, RUN_WAIT))
        return luaL_error(L, "command execution failed.");

    return 0;
}

static int pour_invoke(lua_State* L)
{
    const int pathIndex = 1;
    const char* path = luaL_checkstring(L, pathIndex);

    if (!Dir_IsAbsolutePath(path)) {
        lua_pushstring(L, Script_GetCurrentScriptDir(L));
        lua_pushliteral(L, DIR_SEPARATOR);
        lua_pushvalue(L, pathIndex);
        lua_concat(L, 3);
        path = lua_tostring(L, -1);
    }

    Pour_InvokeScript(L, path);
    return 0;
}

static int pour_shell_open(lua_State* L)
{
    size_t fileLen;
    const char* file = luaL_checklstring(L, 1, &fileLen);

  #ifdef _WIN32

    ++fileLen;
    char* ptr = (char*)lua_newuserdatauv(L, fileLen, 0);
    memcpy(ptr, file, fileLen);
    Dir_ToNativeSeparators(ptr);

    const WCHAR* wfile = (const WCHAR*)Utf8_PushConvertToUtf16(L, ptr, NULL);
    bool result = (INT_PTR)ShellExecuteW(NULL, NULL, wfile, NULL, NULL, SW_SHOWNORMAL) > 32;
    lua_pop(L, 1);

    if (!result)
        luaL_error(L, "unable to open file \"%s\".", file);

  #else

    luaL_error(gL, "shell_open: not implemented on this platform.");

  #endif

    return 0;
}

/********************************************************************************************************************/

static const luaL_Reg funcs[] = {
    { "chdir", pour_chdir },
    { "file_exists", pour_file_exists },
    { "file_read", pour_file_read },
    { "file_write", pour_file_write },
    { "fetch", pour_fetch },
    { "require", pour_require },
    { "run", pour_run },
    { "invoke", pour_invoke },
    { "shell_open", pour_shell_open },
    { NULL, NULL }
};

static int luaopen_pour(lua_State* L)
{
    luaL_newlib(L, funcs);
    return 1;
}

void Pour_InitLua(lua_State* L)
{
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &PACKAGE_DIR);
    lua_setglobal(L, "PACKAGE_DIR");

    luaL_requiref(L, "pour", luaopen_pour, 1);
    lua_pop(L, 1);
}
