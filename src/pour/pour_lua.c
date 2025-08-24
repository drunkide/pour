#include <pour/pour_lua.h>
#include <pour/pour.h>
#include <common/script.h>
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

    if (!File_Exists(file))
        File_CreateDirectory(file);

    if (!File_SetCurrentDirectory(file))
        return luaL_error(L, "can't change directory to \"%s\".", file);

    return 1;
}

static int pour_file_exists(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);
    lua_pushboolean(L, File_Exists(file));
    return 1;
}

static int pour_require(lua_State* L)
{
    const char* package = luaL_checkstring(L, 1);
    if (!Pour_Install(package))
        return luaL_error(L, "could not install required package '%s'.", package);
    return 0;
}

static int pour_run(lua_State* L)
{
    int argc = lua_gettop(L);
    const char* package = luaL_checkstring(L, 1);

    char** argv = (char**)lua_newuserdata(L, argc * sizeof(char**));
    argv[0] = (char*)package;
    for (int i = 1; i < argc; i++)
        argv[i] = (char*)luaL_checkstring(L, i + 1);

    if (!Pour_Run(package, argc, argv))
        return luaL_error(L, "command execution failed.");

    return 0;
}

static int pour_shell_open(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);

  #ifdef _WIN32

    const WCHAR* wfile = (const WCHAR*)Utf8_PushConvertToUtf16(gL, file, NULL);
    bool result = (INT_PTR)ShellExecuteW(NULL, L"open", wfile, NULL, NULL, 0) > 32;
    lua_pop(gL, 1);

    if (!result)
        luaL_error(gL, "unable to open file \"%s\".", file);

  #else

    luaL_error(gL, "shell_open: not implemented on this platform.");

  #endif

    return 0;
}

/********************************************************************************************************************/

static const luaL_Reg funcs[] = {
    { "chdir", pour_chdir },
    { "file_exists", pour_file_exists },
    { "require", pour_require },
    { "run", pour_run },
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
    luaL_requiref(L, "pour", luaopen_pour, 1);
    lua_pop(L, 1);
}
