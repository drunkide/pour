#include <common/common.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <patch/patch.h>

static const char g_table;

static const char* pinned_string(lua_State* L, int index, size_t* len)
{
    const char* str = luaL_checklstring(L, index, len);
    lua_pushvalue(L, index);
    luaL_ref(L, LUA_REGISTRYINDEX);
    return str;
}

static int patch_add(lua_State* L)
{
    size_t searchLen, replacementLen;
    PATCH* patch;

    const char* file = luaL_checkstring(L, 1);
    const char* search = pinned_string(L, 2, &searchLen);
    const char* replacement = pinned_string(L, 3, &replacementLen);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &g_table);
    lua_getfield(L, -1, file);
    if (!lua_isnoneornil(L, -1))
        patch = (PATCH*)lua_touserdata(L, -1);
    else {
        lua_pop(L, 1);
        patch = (PATCH*)lua_newuserdatauv(L, sizeof(PATCH), 0);
        patch->elements = NULL;
        patch->extraBytes = 0;
        lua_setfield(L, -2, file);
    }

    PATCH_ELEMENT* el = (PATCH_ELEMENT*)lua_newuserdatauv(L, sizeof(PATCH_ELEMENT), 0);
    luaL_ref(L, LUA_REGISTRYINDEX);
    el->search = search;
    el->replacement = replacement;
    el->searchLen = searchLen;
    el->replacementLen = replacementLen;

    el->next = patch->elements;
    patch->elements = el;

    if (replacementLen > searchLen)
        patch->extraBytes += (replacementLen - searchLen);

    return 0;
}

PATCH* patch_find(lua_State* L, const char* fileName)
{
    lua_rawgetp(L, LUA_REGISTRYINDEX, &g_table);
    lua_getfield(L, -1, fileName);
    if (lua_isnoneornil(L, -1)) {
        lua_pop(L, 2);
        return NULL;
    }

    PATCH* patch = (PATCH*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return patch;
}

void patch_apply(lua_State* L, const char* fileName, PATCH* patch, void** fileData, unsigned long long* fileSize)
{
    int total = 0;
    int applied = 0;

    char* start = (char*)*fileData;
    char* end = start + *fileSize;

    DONT_WARN_UNUSED(L);

    for (PATCH_ELEMENT* el = patch->elements; el; el = el->next) {
        ++total;
        const char* elEnd = end - el->searchLen;
        for (char* p = start; p < elEnd; ++p) {
            if (!memcmp(p, el->search, el->searchLen)) {
                ++applied;
                if (el->replacementLen != el->searchLen) {
                    memmove(p + el->replacementLen, p + el->searchLen, end - (p + el->searchLen));
                    if (el->replacementLen < el->searchLen) {
                        *fileSize -= (el->searchLen - el->replacementLen);
                        end -= (el->searchLen - el->replacementLen);
                    } else {
                        *fileSize += (el->replacementLen - el->searchLen);
                        end += (el->replacementLen - el->searchLen);
                    }
                }
                memcpy(p, el->replacement, el->replacementLen);
                break;
            }
        }
    }

    printf("\nApplied %d of %d patches to %s\n", applied, total, fileName);
}

static const luaL_Reg funcs[] = {
    { "add", patch_add },
    { NULL, NULL }
};

static int luaopen_patch(lua_State* L)
{
    lua_newtable(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &g_table);

    luaL_newlib(L, funcs);
    return 1;
}

void Patch_InitLua(lua_State* L)
{
    luaL_requiref(L, "patch", luaopen_patch, 1);
    lua_pop(L, 1);
}
