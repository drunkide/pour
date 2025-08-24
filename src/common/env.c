#include <common/env.h>
#include <common/script.h>
#include <common/utf8.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
