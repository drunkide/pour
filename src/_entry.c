#include "main.h"
#include "script.h"
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>

/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain(lua_State *L)
{
    int argc = (int)lua_tointeger(L, 1);
    char** argv = (char**)lua_touserdata(L, 2);

    lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");

    luaL_openlibs(L);

    lua_gc(L, LUA_GCRESTART);  /* start GC... */
    lua_gc(L, LUA_GCGEN, 0, 0);  /* ...in generational mode */

    if (!Main(argc, argv))
        return 0;

    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

int main(int argc, char** argv)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "error: Lua initialization failed.\n");
        return EXIT_FAILURE;
    }

    gL = L;

    lua_gc(L, LUA_GCSTOP);

    lua_pushcfunction(L, &pmain);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);
    int status = lua_pcall(L, 2, 1, 0);
    int result = lua_toboolean(L, -1);
    Script_CheckError(L, status);

    lua_close(L);

    return (result && status == LUA_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}
