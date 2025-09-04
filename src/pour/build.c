#include <pour/build.h>
#include <pour/script.h>
#include <common/console.h>
#include <common/dirs.h>
#include <common/file.h>
#include <common/script.h>
#include <string.h>

/********************************************************************************************************************/

STRUCT(BuildLuaContext)
{
    Target* targetOrNull;
    PFNNAMECALLBACK nameCallback;
    void* nameCallbackData;
};

static void popFunction(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_len(L, 1);
    if (lua_tointeger(L, -1) != 1)
        luaL_error(L, "invalid argument.");
    lua_pop(L, 1);

    lua_rawgeti(L, 1, 1);
    if (!lua_isfunction(L, -1))
        luaL_error(L, "invalid argument.");
}

static int fn_dummy_callback(lua_State* L)
{
    popFunction(L); /* validate argument */
    return 0;
}

static int fn_action_callback(lua_State* L)
{
    popFunction(L);
    lua_call(L, 0, 0);
    return 0;
}

static int fn_target(lua_State* L)
{
    BuildLuaContext* context = (BuildLuaContext*)lua_touserdata(L, lua_upvalueindex(1));
    const char* name = luaL_checkstring(L, 1);

    if (context->nameCallback)
        context->nameCallback(L, name, context->nameCallbackData);

    if (!context->targetOrNull || strcmp(context->targetOrNull->name, name) != 0)
        lua_pushcfunction(L, fn_dummy_callback);
    else
        lua_pushcfunction(L, fn_action_callback);

    return 1;
}

static void popSetSourceAndBuildDir(Target* target)
{
    lua_State* L = target->L;

    const char* sourceDir = lua_tostring(L, -1);
    lua_setfield(L, target->globalsTableIdx, "SOURCE_DIR");

    lua_pushfstring(L, "%s/build/%s-%s", sourceDir, target->platform, target->compiler);
    lua_setfield(L, target->globalsTableIdx, "BUILD_DIR");
}

static bool loadBuildLua(lua_State* L, Target* targetOrNull, PFNNAMECALLBACK callback, void* callbackData)
{
    int n = lua_gettop(L);

    File_PushCurrentDirectory(L);
    size_t currentDirLen;
    const char* currentDir = lua_tolstring(L, -1, &currentDirLen);
    ++currentDirLen;
    char* dir = (char*)lua_newuserdatauv(L, currentDirLen, 0);
    memcpy(dir, currentDir, currentDirLen);
    lua_remove(L, -2);

    do {
        const char* file = lua_pushfstring(L, "%s/Build.lua", dir);
        if (File_Exists(L, file)) {
            int globalsTableIdx;
            if (!targetOrNull)
                globalsTableIdx = Pour_PushNewGlobalsTable(L);
            else {
                globalsTableIdx = targetOrNull->globalsTableIdx;
                lua_pushvalue(L, globalsTableIdx);
            }

            BuildLuaContext* context = (BuildLuaContext*)lua_newuserdatauv(L, sizeof(BuildLuaContext), 0);
            context->targetOrNull = targetOrNull;
            context->nameCallback = callback;
            context->nameCallbackData = callbackData;

            if (targetOrNull) {
                lua_pushstring(L, dir);
                popSetSourceAndBuildDir(targetOrNull);
            }

            lua_pushvalue(L, -1);
            lua_pushcclosure(L, fn_target, 1);
            lua_setfield(L, globalsTableIdx, "target");

            if (!Script_DoFile(L, file, dir, globalsTableIdx))
                luaL_error(L, "error in \"%s\".", file);

            /* in case someone caches 'target' (not supported!) */
            context->targetOrNull = NULL;
            context->nameCallback = NULL;
            context->nameCallbackData = NULL;

            lua_pushnil(L);
            lua_setfield(L, globalsTableIdx, "target");

            lua_settop(L, n);
            return true;
        }
        lua_pop(L, 1);
    } while (Dir_RemoveLastPath(dir));

    Con_PrintF(L, COLOR_WARNING, "WARNING: file \"Build.lua\" was not found, using defaults.");

    if (targetOrNull) {
        File_PushCurrentDirectory(L);
        popSetSourceAndBuildDir(targetOrNull);
    }

    lua_settop(L, n);
    return false;
}

bool Pour_LoadBuildLua(lua_State* L, PFNNAMECALLBACK callback, void* callbackData)
{
    return loadBuildLua(L, NULL, callback, callbackData);
}

/********************************************************************************************************************/

static const char* getString(Target* target, const char* name)
{
    lua_State* L = target->L;
    lua_pushstring(L, name);
    lua_rawget(L, target->globalsTableIdx);
    if (!lua_isstring(L, -1))
        luaL_error(L, "'%s' is not a string.", name);
    return lua_tostring(L, -1);
}

static int getFunction(Target* target, const char* name)
{
    lua_State* L = target->L;

    lua_pushvalue(L, target->globalsTableIdx);
    lua_pushstring(L, name);
    lua_rawget(L, -2);

    if (lua_isnoneornil(L, -1)) {
        lua_pop(L, 2);
        return -1;
    }

    if (!lua_isfunction(L, -1))
        return luaL_error(L, "%s: \"%s\" is not a function.", target->luaScript, name);

    lua_remove(L, -2);
    return lua_gettop(L);
}

bool Pour_LoadTarget(lua_State* L, Target* target, const char* name)
{
    target->L = L;

    /* parse target name */

    target->platform = NULL;
    target->compiler = NULL;

    size_t targetLen = strlen(name) + 1;
    char* targetPath = (char*)lua_newuserdatauv(L, targetLen, 0);
    memcpy(targetPath, name, targetLen);
    for (char* p = targetPath; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected '%c' in target name \"%s\".", *p, name);
            return false;
        }
        if (*p == ':') {
            if (target->platform) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected '%c' in target name \"%s\".", *p, name);
                return false;
            }
            target->platform = lua_pushlstring(L, targetPath, (size_t)(p - targetPath));
            target->compiler = p + 1;
        }
    }

    if (!target->platform) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: missing '%c' in target name \"%s\".", ':', name);
        return false;
    }

    target->luaScriptDir = lua_pushfstring(L, "%s/%s", g_targetsDir, target->platform);
    target->luaScript = lua_pushfstring(L, "%s/%s.lua", target->luaScriptDir, target->compiler);
    if (!File_Exists(L, target->luaScript)) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unknown target \"%s\".", name);
        return false;
    }

    /* setup defaults */

    target->globalsTableIdx = Pour_PushNewGlobalsTable(L);

    lua_pushstring(L, name);
    target->name = lua_tostring(L, -1);
    lua_setfield(L, -2, "TARGET");

    lua_pushstring(L, target->platform);
    lua_setfield(L, -2, "TARGET_PLATFORM");

    lua_pushstring(L, target->compiler);
    lua_setfield(L, -2, "TARGET_COMPILER");

    /* load target lua */

    if (!Script_DoFile(L, target->luaScript, NULL, target->globalsTableIdx))
        return false;

    target->prepareFn = getFunction(target, "prepare");
    target->generateFn = getFunction(target, "generate");
    target->buildFn = getFunction(target, "build");

    if (target->prepareFn < 0 && target->generateFn < 0 && target->buildFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is not supported on current platform.", target->name);
        return false;
    }

    if (target->buildFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is missing the \"%s\" function.", target->name, "build");
        return false;
    }
    if (target->generateFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is missing the \"%s\" function.", target->name, "generate");
        return false;
    }

    /* prepare */

    if (target->prepareFn > 0) {
        lua_pushvalue(L, target->prepareFn);
        lua_call(L, 0, 0);
    }

    /* load Build.lua */

    loadBuildLua(L, target, NULL, NULL);

    /* read configuration */

    target->sourceDir = getString(target, "SOURCE_DIR");
    target->buildDir = getString(target, "BUILD_DIR");

    return true;
}

/********************************************************************************************************************/

bool Pour_GenerateTarget(Target* target, genmode_t mode)
{
    lua_State* L = target->L;

    const char* generated = lua_pushfstring(L, "%s/%s", target->buildDir, ".generated");
    if (mode == GEN_NORMAL) {
        if (File_Exists(L, generated))
            return true;

        /* if generation failed previously, force full rebuild */
        mode = GEN_FORCE_REBUILD;
    }

    if (mode == GEN_FORCE_REBUILD) {
        const char* CMakeCache_txt = lua_pushfstring(L, "%s/%s", target->buildDir, "CMakeCache.txt");
        if (File_Exists(L, CMakeCache_txt))
            File_TryDelete(L, CMakeCache_txt);
    }

    if (!Script_DoFunction(L, target->luaScriptDir, target->buildDir, target->generateFn))
        return false;

    if (!File_Exists(L, generated))
        File_Overwrite(L, generated, "", 0);

    return true;
}

bool Pour_BuildTarget(Target* target)
{
    lua_State* L = target->L;
    return Script_DoFunction(L, target->luaScriptDir, target->buildDir, target->buildFn);
}

/********************************************************************************************************************/

bool Pour_Build(lua_State* L, const char* targetName, buildmode_t mode)
{
    int n = lua_gettop(L);

    Target target;
    if (!Pour_LoadTarget(L, &target, targetName)) {
        lua_settop(L, n);
        return false;
    }

    genmode_t genmode = GEN_NORMAL;
    if (mode == BUILD_GENERATE_ONLY)
        genmode = GEN_FORCE;
    else if (mode == BUILD_GENERATE_ONLY_FORCE)
        genmode = GEN_FORCE_REBUILD;

    if (!Pour_GenerateTarget(&target, genmode)) {
        lua_settop(L, n);
        return false;
    }

    switch (mode) {
        case BUILD_GENERATE_ONLY:
        case BUILD_GENERATE_ONLY_FORCE:
            break;

        case BUILD_NORMAL:
            if (!Pour_BuildTarget(&target)) {
                lua_settop(L, n);
                return false;
            }
            break;

        case BUILD_GENERATE_AND_OPEN: {
            const char* CMakeCache_txt = lua_pushfstring(L, "%s/%s", target.buildDir, "CMakeCache.txt");
            if (!File_Exists(L, CMakeCache_txt)) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: file \"%s\" was not found.", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            const char* data = File_PushContents(L, CMakeCache_txt, NULL);

            static const char param[] = "CMAKE_PROJECT_NAME:STATIC=";
            const char* p = strstr(data, param);
            if (!p) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: project name was not found in \"%s\".", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            p += sizeof(param) - 1;
            const char* end = strpbrk(p, "\r\n");
            if (!end || end == p) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: can't extract project name from \"%s\".", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            lua_pushlstring(L, p, (size_t)(end - p));

            lua_pushliteral(L, ".sln");
            lua_concat(L, 2);
            const char* slnFile = lua_tostring(L, -1);
            if (File_Exists(L, slnFile)) {
                File_ShellOpen(L, slnFile);
                break;
            }

            Con_PrintF(L, COLOR_ERROR, "ERROR: file not found: \"%s\".", slnFile);
            lua_settop(L, n);
            return false;
        }
    }

    lua_settop(L, n);
    return true;
}
