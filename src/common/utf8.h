#ifndef COMMON_UTF8_H
#define COMMON_UTF8_H

#include <common/common.h>
#include <stdint.h>

const char* Utf8_PushConvertFromUtf16(lua_State*L, const void* utf16);
const uint16_t* Utf8_PushConvertToUtf16(lua_State* L, const char* utf8);

#endif
