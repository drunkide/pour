#include <common/env.h>
#include <common/script.h>
#include <common/utf8.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const char* Env_PushGet(lua_State* L, const char* variable)
{
  #ifdef _WIN32

    DWORD dwError;

    const WCHAR* name16 = (const WCHAR*)Utf8_PushConvertToUtf16(L, variable, NULL);

    WCHAR buf[256], *p;
    DWORD len = GetEnvironmentVariableW(name16, buf, sizeof(buf) / sizeof(buf[0]));
    if (len == 0) {
        dwError = GetLastError();
        lua_pop(L, 1);
      failure:
        if (dwError != ERROR_ENVVAR_NOT_FOUND) {
            luaL_error(L, "error reading environment variable \"%s\" (code %p).",
                variable, (void*)(size_t)dwError);
        }
        return NULL;
    }

    p = buf;
    if (len > sizeof(buf) / sizeof(buf[0])) {
        p = (WCHAR*)lua_newuserdatauv(L, len * sizeof(WCHAR), 0);
        len = GetEnvironmentVariableW(name16, p, len);
        if (len == 0) {
            dwError = GetLastError();
            lua_pop(L, 2);
            goto failure;
        }
    }

    const char* result = Utf8_PushConvertFromUtf16(L, p);

    if (p == buf)
        lua_replace(L, -2);
    else {
        lua_replace(L, -3);
        lua_pop(L, 1);
    }

    return result;

  #else

    const char* env = getenv(variable);
    if (!env)
        return NULL;

    lua_pushstring(L, env);
    return lua_tostring(L, -1);

  #endif
}

void Env_Set(lua_State* L, const char* variable, const char* value)
{
  #ifdef _WIN32

    int n = lua_gettop(L);

    const WCHAR* name16 = (const WCHAR*)Utf8_PushConvertToUtf16(L, variable, NULL);
    const WCHAR* value16 = (const WCHAR*)Utf8_PushConvertToUtf16(L, value, NULL);
    if (!SetEnvironmentVariableW(name16, value16))
        luaL_error(L, "unable to update PATH environment variable.");

    lua_settop(L, n);

  #else

    if (setenv(variable, value, 1) != 0)
        luaL_error(L, "unable to set environment variable '%s'.", variable);

  #endif
}

void Env_PrependPath(lua_State* L, const char* path)
{
    int n = lua_gettop(L);

    lua_pushstring(L, path);
  #ifdef _WIN32
    lua_pushliteral(L, ";");
  #else
    lua_pushliteral(L, ":");
  #endif
    if (Env_PushGet(L, "PATH"))
        lua_concat(L, 3);
    else
        lua_pop(L, 1);

    Env_Set(L, "PATH", lua_tostring(L, -1));

    lua_settop(L, n);
}
