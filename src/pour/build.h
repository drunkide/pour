#ifndef POUR_BUILD_H
#define POUR_BUILD_H

#include <pour/pour.h>

STRUCT(Target) {
    lua_State* L;
    const char* name;
    const char* platform;
    const char* compiler;
    const char* configuration;
    const char* luaScript;
    const char* luaScriptDir;
    const char* buildDir;
    const char* sourceDir;
    const char* cmakeGenerator;
    const char* cmakeVersion;
    int globalsTableIdx;
    int prepareFn;
    int generateFn;
    int buildFn;
    bool isMulticonfig;
};

typedef enum genmode_t {
    GEN_NORMAL = 0,
    GEN_FORCE,
    GEN_FORCE_REBUILD,
} genmode_t;

typedef enum buildmode_t {
    BUILD_NORMAL,
    BUILD_REBUILD,
    BUILD_GENERATE_ONLY,
    BUILD_GENERATE_ONLY_FORCE,
    BUILD_GENERATE_AND_OPEN,
} buildmode_t;

typedef void (*PFNNAMECALLBACK)(lua_State* L, const char* name, void* data);

void Pour_LoadBuildLua(lua_State* L, PFNNAMECALLBACK callback, void* callbackData);

bool Pour_LoadTarget(lua_State* L, Target* target, const char* name);
bool Pour_GenerateTarget(Target* target, genmode_t mode);
bool Pour_BuildTarget(Target* target, bool cleanFirst);

bool Pour_Build(lua_State* L, const char* targetName, buildmode_t mode);

#endif
