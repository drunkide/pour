#include <common/env.h>
#include <common/script.h>
#include <common/utf8.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void Env_PrependPath(const char* path)
{
  #ifdef _WIN32

    lua_State* L = gL;
    int n = lua_gettop(L);

    WCHAR src[32768]; /* maximum limit of an environment variable */
    GetEnvironmentVariableW(L"PATH", src, sizeof(src) / sizeof(src[0]));

    lua_pushstring(L, path);
    lua_pushliteral(L, ";");
    Utf8_PushConvertFromUtf16(L, src);
    lua_concat(L, 3);

    size_t dstLen = 0;
    const char* dst = lua_tolstring(L, -1, &dstLen);

    const uint16_t* newPath = Utf8_PushConvertToUtf16(L, dst, NULL);
    SetEnvironmentVariableW(L"PATH", newPath);

    lua_settop(L, n);

  #else

    /* FIXME: not implemented */
    abort();

  #endif
}
