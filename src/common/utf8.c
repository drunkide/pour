#include <common/utf8.h>

/*****************************************************************************
 * Based on Flexible and Economical UTF-8 Decoder.
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 * Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 ****************************************************************************/

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] = {
        /* The first part of the table maps bytes to character classes that
           to reduce the size of the transition table and create bitmasks. */
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

        /* The second part is a transition table that maps a combination
           of a state of the automaton and a character class to a state. */
         0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
        12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
        12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
        12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
        12,36,12,12,12,12,12,12,12,12,12,12,
    };

static bool Utf8_GetNextUtf32(const char** pSrc, uint32_t* codepoint)
{
    const char* src = *pSrc;
    uint8_t state = 0;
    uint8_t type, ch;
    uint32_t codep = 0;

    for (;;) {
        ch = (uint8_t)*src;
        if (!ch) {
            *pSrc = src;
            return false;
        }
        ++src;

        type = utf8d[ch];
        codep = (state != UTF8_ACCEPT ? ((ch & 0x3fu) | (codep << 6)) : (uint32_t)(ch & (0xff >> type)));
        state = utf8d[256 + state + type];

        if (state == UTF8_ACCEPT) {
            *codepoint = codep;
            goto ret;
        }

        if (state == UTF8_REJECT) {
            if (src > *pSrc + 1)
                --src;
            *codepoint = 0xFFFD; /* unicode replacement character */
          ret:
            *pSrc = src;
            return true;
        }
    }
}

/*****************************************************************************
 * ^^ End on Flexible and Economical UTF-8 Decoder. */

const char* Utf8_PushConvertFromUtf16(lua_State*L, const void* utf16)
{
    const uint16_t* src = (const uint16_t*)utf16;
    uint8_t bytes[4];

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    while (*src) {
        uint32_t codep = *src++;
        if (codep >= 0xD800 && codep <= 0xDBFF) {
            uint16_t u16 = *src;
            if (u16 >= 0xDC00 && u16 <= 0xDFFF) {
                ++src;
                uint32_t high = (uint32_t)(codep - 0xD800);
                uint32_t low = (uint32_t)(u16 - 0xDC00);
                codep = 0x10000 + ((high << 10) | low);
            } else
                codep = 0xFFFD;
        }

        uint8_t *dst = bytes;
        if (codep < 0x80)
            *dst++ = (uint8_t)codep;
        else if (codep < 0x800) {
            *dst++ = (uint8_t)(0xC0 | (codep >> 6));
            *dst++ = (uint8_t)(0x80 | (codep & 0x3F));
        } else if (codep < 0x10000) {
            *dst++ = (uint8_t)(0xE0 | ( codep >> 12));
            *dst++ = (uint8_t)(0x80 | ((codep >>  6) & 0x3F));
            *dst++ = (uint8_t)(0x80 | ( codep & 0x3F));
        } else {
            *dst++ = (uint8_t)(0xF0 | ( codep >> 18));
            *dst++ = (uint8_t)(0x80 | ((codep >> 12) & 0x3F));
            *dst++ = (uint8_t)(0x80 | ((codep >>  6) & 0x3F));
            *dst++ = (uint8_t)(0x80 | ( codep & 0x3F));
        }

        luaL_addlstring(&buf, (char*)bytes, dst - bytes);
    }

    return luaL_pushresult(&buf);
}

const uint16_t* Utf8_PushConvertToUtf16(lua_State* L, const char* utf8)
{
    luaL_Buffer buf;
    uint32_t codep;

    luaL_buffinit(L, &buf);

    while (Utf8_GetNextUtf32(&utf8, &codep)) {
        uint16_t u[2];
        if (codep < 0x10000) {
            u[0] = (uint16_t)codep;
            luaL_addlstring(&buf, (char*)&u[0], sizeof(u[0]));
        } else {
            u[0] = (uint16_t)(0xD7C0 + (codep >> 10));
            u[1] = (uint16_t)(0xDC00 + (codep & 0x3FF));
            luaL_addlstring(&buf, (char*)u, sizeof(u));
        }
    }

    codep = 0;
    luaL_addlstring(&buf, (char*)&codep, sizeof(uint16_t));

    return (const uint16_t*)luaL_pushresult(&buf);
}
