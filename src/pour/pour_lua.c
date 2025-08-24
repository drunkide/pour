#include <pour/pour_lua.h>
#include <pour/pour.h>

static int pour_require(lua_State* L)
{
    const char* package = luaL_checkstring(L, 1);
    if (!Pour_Install(package))
        return luaL_error(L, "could not install required package '%s'.", package);
    return 0;
}

/********************************************************************************************************************/

static const luaL_Reg funcs[] = {
    { "require", pour_require },
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
