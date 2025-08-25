#include <common/env.h>
#include <common/script.h>
#include <common/utf8.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const char* Env_PushGet(const char* variable)
{
    lua_State* L = gL;

  #ifdef _WIN32

    const WCHAR* name16 = (const WCHAR*)Utf8_PushConvertToUtf16(L, variable, NULL);

    WCHAR buf[32768];
    DWORD len = GetEnvironmentVariableW(name16, buf, sizeof(buf) / sizeof(buf[0]));
    lua_pop(L, 1);

    if (len == 0 || len > sizeof(buf) / sizeof(buf[0]))
        return NULL;

    return Utf8_PushConvertFromUtf16(L, buf);

  #else

    const char* env = getenv(variable);
    if (!env)
        return NULL;

    lua_pushstring(L, env);
    return lua_tostring(L, -1);

  #endif
}

void Env_Set(const char* variable, const char* value)
{
    lua_State* L = gL;

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

void Env_PrependPath(const char* path)
{
    lua_State* L = gL;

  #ifdef _WIN32

    int n = lua_gettop(L);

    WCHAR src[32768]; /* maximum limit of an environment variable */
    DWORD sz = GetEnvironmentVariableW(L"PATH", src, sizeof(src) / sizeof(src[0]));
    if (!sz || sz > sizeof(src))
        luaL_error(L, "unable to get PATH environment variable.");

    lua_pushstring(L, path);
    lua_pushliteral(L, ";");
    Utf8_PushConvertFromUtf16(L, src);
    lua_concat(L, 3);

    size_t dstLen = 0;
    const char* dst = lua_tolstring(L, -1, &dstLen);

    const WCHAR* newPath = (const WCHAR*)Utf8_PushConvertToUtf16(L, dst, NULL);
    if (!SetEnvironmentVariableW(L"PATH", newPath))
        luaL_error(L, "unable to update PATH environment variable.");

    lua_settop(L, n);

  #else

    /* FIXME: not implemented */
    luaL_error(L, "Env_PrependPath: not implemented on this platform.");

  #endif
}
