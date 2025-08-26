#ifndef PATCH_PATCH_H
#define PATCH_PATCH_H

#include <common/common.h>

STRUCT(PATCH_ELEMENT) {
    struct PATCH_ELEMENT* next;
    const char* search;
    const char* replacement;
    size_t searchLen;
    size_t replacementLen;
};

STRUCT(PATCH) {
    PATCH_ELEMENT* elements;
    size_t extraBytes;
};

PATCH* patch_find(lua_State* L, const char* fileName);
void patch_apply(lua_State* L, const char* fileName, PATCH* patch, void** fileData, unsigned long long* fileSize);

void Patch_InitLua(lua_State* L);

#endif
