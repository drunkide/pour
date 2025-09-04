#include <pour/script.h>
#include <common/script.h>
#include <common/dirs.h>
#include <string.h>

char ARG;

/********************************************************************************************************************/

int Pour_PushNewGlobalsTable(lua_State* L)
{
    lua_newtable(L);

    lua_pushstring(L, g_pourExecutable);
    lua_setfield(L, -2, "POUR_EXECUTABLE");

  #ifdef _WIN32
    lua_pushboolean(L, 1);
    lua_setglobal(L, "HOST_WINDOWS");
  #endif

    return lua_gettop(L);
}

bool Pour_ExecScript(lua_State* L, const char* script, const char* chdir, int argc, char** argv)
{
    int n = lua_gettop(L);

    int globalsTableIdx = Pour_PushNewGlobalsTable(L);

    lua_createtable(L, argc, argc);
    for (int i = 1, idx = 1; i < argc; i++) {
        const char* p = strchr(argv[i], '=');
        if (p) {
            lua_pushlstring(L, argv[i], p - argv[i]);

            ++p;
            if (*p != '{')
                lua_pushstring(L, p);
            else {
                lua_pushfstring(L, "return %s", p);
                if (luaL_loadstring(L, lua_tostring(L, -1)) != 0)
                    luaL_error(L, "syntax error in expression: %s", p);
                lua_remove(L, -2);
                if (lua_pcall(L, 0, 1, 0))
                    luaL_error(L, "error evaluating expression: %s", p);
            }

            lua_rawset(L, -3);
            continue;
        }

        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, idx++);
    }

    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ARG);
    lua_setfield(L, globalsTableIdx, "arg");

    if (!Script_DoFile(L, script, chdir, globalsTableIdx)) {
        lua_settop(L, n);
        return false;
    }

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

void Pour_InvokeScript(lua_State* L, const char* script)
{
    int n = lua_gettop(L);

    size_t len = strlen(script) + 1;
    char* dir = (char*)lua_newuserdata(L, len);
    memcpy(dir, script, len);
    if (!Dir_RemoveLastPath(dir))
        dir = NULL;

    int globalsTableIdx = Pour_PushNewGlobalsTable(L);
    lua_rawgetp(L, LUA_REGISTRYINDEX, &ARG);
    lua_setfield(L, -2, "arg");

    if (!Script_DoFile(L, script, dir, globalsTableIdx))
        luaL_error(L, "execution of script \"%s\" failed.", script);

    lua_settop(L, n);
}
