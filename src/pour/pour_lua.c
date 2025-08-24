#include <pour/pour_lua.h>
#include <pour/pour.h>

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

/********************************************************************************************************************/

static const luaL_Reg funcs[] = {
    { "require", pour_require },
    { "run", pour_run },
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
