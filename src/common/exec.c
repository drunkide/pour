#include <common/exec.h>
#include <common/dirs.h>
#include <common/script.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

static void pushArgument(lua_State* L, const char* argument)
{
    bool needEscape = false;

    for (const char* p = argument; *p; ++p) {
        if (*p == ' ') {
            needEscape = true;
            break;
        }
    }

    if (needEscape)
        lua_pushliteral(L, "\"");
    lua_pushstring(L, argument);
    if (needEscape)
        lua_pushliteral(L, "\"");
}

bool Exec_Command(const char* const* argv, int argc)
{
    return Exec_CommandV(argv[0], argv, argc);
}

bool Exec_CommandV(const char* command, const char* const* argv, int argc)
{
    lua_State* L = gL;
    luaL_checkstack(L, 100, NULL);

  #ifdef _WIN32
    size_t commandLen = strlen(command);
    char* commandBuf = (char*)alloca(commandLen + 1);
    memcpy(commandBuf, command, commandLen);
    commandBuf[commandLen] = 0;
    Dir_ToNativeSeparators(commandBuf);
    command = commandBuf;
  #endif

    int start = lua_gettop(L);
    lua_pushliteral(L, "cmd /C ");
    pushArgument(L, command);
    for (int i = 1; i < argc; i++) {
        lua_pushliteral(L, " ");
        pushArgument(L, argv[i]);
    }
    lua_concat(L, lua_gettop(L) - start);
    const char* cmd = lua_tostring(L, -1);

    printf("# %s\n", cmd);
    if (system(cmd) != 0) {
        lua_settop(L, start);
        return false;
    }

    lua_settop(L, start);
    return true;
}
