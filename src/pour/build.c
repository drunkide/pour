#include <pour/build.h>
#include <pour/run.h>
#include <pour/install.h>
#include <pour/script.h>
#include <common/console.h>
#include <common/dirs.h>
#include <common/file.h>
#include <common/script.h>
#include <ctype.h>
#include <string.h>

/********************************************************************************************************************/

STRUCT(BuildLuaContext)
{
    Target* targetOrNull;
    PFNNAMECALLBACK nameCallback;
    void* nameCallbackData;
    bool matched;
};

static void readSettings(lua_State* L, Target* targetOrNull, int globalsTableIdx)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_isfunction(L, -1)) {
            if (!targetOrNull)
                lua_pop(L, 1);
            else
                lua_call(L, 0, 0);
        } else if (lua_isstring(L, -1) || lua_isinteger(L, -1) || lua_isboolean(L, -1)) {
            const char* key = luaL_checkstring(L, -2);
            if (!targetOrNull)
                lua_pop(L, 1);
            else {
                lua_getfield(L, globalsTableIdx, key);
                if (lua_istable(L, -1))
                    luaL_error(L, "\"%s\" is a table.", key);
                lua_pop(L, 1);
                lua_setfield(L, globalsTableIdx, key);
            }
        } else if (lua_istable(L, -1)) {
            const char* key = luaL_checkstring(L, -2);
            if (targetOrNull) {
                lua_getfield(L, globalsTableIdx, "table_append");
                lua_getfield(L, globalsTableIdx, key);
                if (!lua_istable(L, -1))
                    luaL_error(L, "\"%s\" is not a table.", key);
                lua_pushvalue(L, -3);
                lua_call(L, 2, 0);
            }
            lua_pop(L, 1);
        } else
            luaL_error(L, "invalid argument.");
    }
}

static int fn_dummy_callback(lua_State* L)
{
    readSettings(L, NULL, 0); /* just validate argument */
    return 0;
}

static int fn_action_callback(lua_State* L)
{
    BuildLuaContext* context = (BuildLuaContext*)lua_touserdata(L, lua_upvalueindex(1));
    int globalsTableIdx = lua_upvalueindex(2);
    readSettings(L, context->targetOrNull, globalsTableIdx);
    return 0;
}

static int fn_target(lua_State* L)
{
    BuildLuaContext* context = (BuildLuaContext*)lua_touserdata(L, lua_upvalueindex(1));
    int globalsTableIdx = lua_upvalueindex(2);
    const char* name = luaL_checkstring(L, 1);

    if (context->nameCallback)
        context->nameCallback(L, name, context->nameCallbackData);

    if (context->targetOrNull) {
        Target* target = context->targetOrNull;

        if (!strcmp(target->shortName, name))
            goto match;

        if (!strcmp(target->name, name)) {
            if (target->isMulticonfig) {
                luaL_error(L, "full target name (\"%s\") should not be used for multiconfig generators "
                    "like Visual Studio and Xcode. Use short target name (\"%s\") instead.",
                    target->name, target->shortName);
            }
          match:
            context->matched = true;
            lua_pushlightuserdata(L, context);
            lua_pushvalue(L, globalsTableIdx);
            lua_pushcclosure(L, fn_action_callback, 2);
            return 1;
        }
    }

    lua_pushcfunction(L, fn_dummy_callback);
    return 1;
}

static void pushDefaultSourceDir(lua_State* L, const char* sourceDir)
{
    if (sourceDir)
        lua_pushstring(L, sourceDir);
    else
        File_PushCurrentDirectory(L);
}

static void popSetSourceAndBuildDir(Target* target)
{
    lua_State* L = target->L;

    const char* sourceDir = lua_tostring(L, -1);
    lua_setfield(L, target->globalsTableIdx, "SOURCE_DIR");

    lua_pushfstring(L, "%s/build/%s-%s", sourceDir, target->platform, target->compiler);
    if (!target->isMulticonfig) {
        lua_pushliteral(L, "-");
        lua_pushstring(L, target->configuration);
        lua_concat(L, 3);
    }
    lua_setfield(L, target->globalsTableIdx, "BUILD_DIR");
}

static bool loadBuildLua(lua_State* L, Target* targetOrNull,
    const char* sourceDir, PFNNAMECALLBACK callback, void* callbackData)
{
    int n = lua_gettop(L);

    pushDefaultSourceDir(L, sourceDir);
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
            context->matched = false;

            if (targetOrNull) {
                lua_pushstring(L, dir);
                popSetSourceAndBuildDir(targetOrNull);
            }

            lua_pushvalue(L, -1);
            lua_pushvalue(L, globalsTableIdx);
            lua_pushcclosure(L, fn_target, 2);
            lua_setfield(L, globalsTableIdx, "target");

            if (!Script_DoFile(L, file, dir, globalsTableIdx))
                luaL_error(L, "error in \"%s\".", file);

            bool matched = context->matched;

            /* in case someone caches 'target' (not supported!) */
            context->targetOrNull = NULL;
            context->nameCallback = NULL;
            context->nameCallbackData = NULL;

            lua_pushnil(L);
            lua_setfield(L, globalsTableIdx, "target");

            lua_settop(L, n);
            return matched;
        }
        lua_pop(L, 1);
    } while (Dir_RemoveLastPath(dir));

    Con_PrintF(L, COLOR_WARNING, "WARNING: file \"Build.lua\" was not found, using defaults.\n");

    if (targetOrNull) {
        pushDefaultSourceDir(L, sourceDir);
        popSetSourceAndBuildDir(targetOrNull);
    }

    lua_settop(L, n);
    return true;
}

void Pour_LoadBuildLua(lua_State* L, const char* sourceDir, PFNNAMECALLBACK callback, void* callbackData)
{
    loadBuildLua(L, NULL, sourceDir, callback, callbackData);
}

/********************************************************************************************************************/

static const char* getString(Target* target, const char* name)
{
    lua_State* L = target->L;
    lua_pushstring(L, name);
    lua_rawget(L, target->globalsTableIdx);
    if (lua_isnoneornil(L, -1))
        luaL_error(L, "required variable '%s' was not defined.", name);
    if (!lua_isstring(L, -1))
        luaL_error(L, "'%s' is not a string.", name);
    const char* str = lua_tostring(L, -1);
    lua_rawsetp(L, target->globalsTableIdx, str); /* preserve string */
    return str;
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

static void setGlobals(Target* target)
{
    lua_State* L = target->L;

    lua_pushstring(L, target->name);
    lua_setfield(L, target->globalsTableIdx, "TARGET");

    lua_pushstring(L, target->platform);
    lua_setfield(L, target->globalsTableIdx, "TARGET_PLATFORM");

    lua_pushstring(L, target->compiler);
    lua_setfield(L, target->globalsTableIdx, "TARGET_COMPILER");

    lua_pushstring(L, target->configuration);
    lua_setfield(L, target->globalsTableIdx, "TARGET_CONFIGURATION");

    if (!target->configuration)
        lua_pushnil(L);
    else {
        char upper = toupper(*target->configuration);
        lua_pushlstring(L, &upper, 1);
        lua_pushstring(L, target->configuration + 1);
        lua_concat(L, 2);
    }
    lua_setfield(L, target->globalsTableIdx, "CMAKE_CONFIGURATION");

    lua_pushstring(L, target->cmakeGenerator);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_GENERATOR");

    lua_pushboolean(L, target->isMulticonfig);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_IS_MULTICONFIG");

    lua_pushboolean(L, g_verbose);
    lua_setfield(L, target->globalsTableIdx, "VERBOSE");
}

bool Pour_LoadTarget(lua_State* L, Target* target, const char* sourceDir, const char* name)
{
    target->L = L;

    /* parse target name */

    target->platform = NULL;
    target->compiler = NULL;
    target->configuration = NULL;

    size_t targetLen = strlen(name) + 1;
    char* targetPath = (char*)lua_newuserdatauv(L, targetLen, 0);
    memcpy(targetPath, name, targetLen);
    for (char* p = targetPath; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected '%c' in target name \"%s\".\n", *p, name);
            return false;
        }
        if (*p == ':') {
            if (!target->platform) {
                target->platform = lua_pushlstring(L, targetPath, (size_t)(p - targetPath));
                target->compiler = p + 1;
                continue;
            }
            if (!target->configuration) {
                target->compiler = lua_pushlstring(L, target->compiler, (size_t)(p - target->compiler));
                target->configuration = p + 1;
                continue;
            }
            Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected '%c' in target name \"%s\".\n", *p, name);
            return false;
        }
    }

    if (!target->platform) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: missing '%c' in target name \"%s\".\n", ':', name);
        return false;
    }

    if (target->configuration) {
        if (strcmp(target->configuration, "debug") != 0 &&
                strcmp(target->configuration, "release") != 0 &&
                strcmp(target->configuration, "relwithdebinfo") != 0 &&
                strcmp(target->configuration, "minsizerel") != 0) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: invalid configuration \"%s\" in target name \"%s\".\n", target->configuration, name);
            return false;
        }
    }

    target->luaScriptDir = lua_pushfstring(L, "%s/%s", g_targetsDir, target->platform);
    target->luaScript = lua_pushfstring(L, "%s/%s.lua", target->luaScriptDir, target->compiler);
    if (!File_Exists(L, target->luaScript)) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unknown target \"%s\".\n", name);
        return false;
    }

    lua_pushstring(L, name);
    target->name = lua_tostring(L, -1);

    if (!target->configuration)
        target->shortName = target->name;
    else {
        lua_pushfstring(L, "%s:%s", target->platform, target->compiler);
        target->shortName = lua_tostring(L, -1);
    }

    /* setup defaults */

    target->globalsTableIdx = Pour_PushNewGlobalsTable(L);
    target->cmakeGenerator = "";
    target->isMulticonfig = false;

    setGlobals(target);

    lua_newtable(L);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_PARAMS");

    lua_newtable(L);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_BUILD_PARAMS");

    lua_createtable(L, 1, 0);
    lua_pushstring(L, g_cmakeModulesDir);
    lua_rawseti(L, -2, 1);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_MODULE_PATH");

    /* load target lua */

    if (!Script_DoFile(L, target->luaScript, NULL, target->globalsTableIdx))
        return false;

    target->prepareFn = getFunction(target, "prepare");
    target->generateFn = getFunction(target, "generate");
    target->buildFn = getFunction(target, "build");

    if (target->prepareFn < 0 && target->generateFn < 0 && target->buildFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is not supported on current platform.\n", target->name);
        return false;
    }

    if (target->buildFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is missing the \"%s\" function.\n", target->name, "build");
        return false;
    }
    if (target->generateFn < 0) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: target \"%s\" is missing the \"%s\" function.\n", target->name, "generate");
        return false;
    }

    /* validate CMake generator and install necessary packages */

    target->cmakeGenerator = getString(target, "CMAKE_GENERATOR");

    if (!strcmp(target->cmakeGenerator, "Ninja")) {
        target->isMulticonfig = false;
        if (!Pour_Install(L, "ninja", false))
            return false;
    } else if (!strcmp(target->cmakeGenerator, "MinGW Makefiles")) {
        target->isMulticonfig = false;
        if (!Pour_Install(L, "make", false))
            return false;
    } else if (!strcmp(target->cmakeGenerator, "NMake Makefiles")) {
        target->isMulticonfig = false;
    } else if (!strcmp(target->cmakeGenerator, "Xcode")) {
        target->isMulticonfig = true;
    } else if (!strcmp(target->cmakeGenerator, "Visual Studio 17 2022")) {
        target->isMulticonfig = true;
    } else {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unsupported CMake generator \"%s\".\n", target->cmakeGenerator);
        return false;
    }

    /* ensure CMAKE_BUILD_TYPE */

    if (!target->isMulticonfig && !target->configuration) {
        target->configuration = "release";
        Con_PrintF(L, COLOR_WARNING,
            "WARNING: configuration was not specified for target \"%s\", building \"%s\".\n",
            target->name, target->configuration);

        target->name = lua_pushfstring(L, "%s:%s", target->name, target->configuration);
    }

    /* prepare */

    if (target->prepareFn > 0) {
        setGlobals(target);
        lua_pushvalue(L, target->prepareFn);
        lua_call(L, 0, 0);
    }

    /* load Build.lua */

    setGlobals(target);
    if (!loadBuildLua(L, target, sourceDir, NULL, NULL)) {
        if (!strcmp(target->name, target->shortName)) {
            Con_PrintF(L, COLOR_ERROR, "ERROR: \"%s\" was not found in Build.lua.\n",
                target->name, target->shortName);
        } else {
            Con_PrintF(L, COLOR_ERROR, "ERROR: neither \"%s\" nor \"%s\" was found in Build.lua.\n",
                target->name, target->shortName);
        }
        return false;
    }

    /* read configuration */

    target->sourceDir = getString(target, "SOURCE_DIR");
    target->buildDir = getString(target, "BUILD_DIR");
    target->cmakeVersion = getString(target, "CMAKE_VERSION");

    return true;
}

/********************************************************************************************************************/

static int cmake_generate(lua_State* L)
{
    Target* target = (Target*)lua_touserdata(L, lua_upvalueindex(1));
    genmode_t mode = (genmode_t)lua_tointeger(L, lua_upvalueindex(2));
    int argc = lua_gettop(L);

    const char* cmakeVersion = luaL_checkstring(L, 1);
    const char* cmake = lua_pushfstring(L, "cmake-%s", cmakeVersion);

    char** argv = (char**)lua_newuserdatauv(L, argc * sizeof(char**), 0);
    argv[0] = (char*)cmake;

    size_t len = strlen(argv[0]) + 1;
    for (int i = 1; i < argc; i++) {
        size_t argLen;
        char* arg = (char*)luaL_checklstring(L, i + 1, &argLen);
        argv[i] = arg;
        len += argLen + 1;
    }

    char* buffer = (char*)lua_newuserdatauv(L, len, 0);
    char* p = buffer;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        memcpy(p, argv[i], len);
        argv[i] = p;
        p += len;
    }

    const char* generated = lua_pushfstring(L, "%s/%s", target->buildDir, ".pour-generated");
    if (mode == GEN_NORMAL) {
        if (File_Exists(L, generated)) {
            size_t dataLen;
            const char* data = File_PushContents(L, generated, &dataLen);
            if (dataLen == len && !memcmp(data, buffer, len))
                return 0;
        }

        /* if generation failed previously or options changed, force full rebuild */
        mode = GEN_FORCE_REBUILD;
    }

    if (mode == GEN_FORCE_REBUILD) {
        const char* CMakeCache_txt = lua_pushfstring(L, "%s/%s", target->buildDir, "CMakeCache.txt");
        if (File_Exists(L, CMakeCache_txt))
            File_TryDelete(L, CMakeCache_txt);
    }

    if (!Pour_Run(L, cmake, NULL, argc, argv, RUN_WAIT)) {
        if (File_Exists(L, generated))
            File_TryDelete(L, generated);
        return luaL_error(L, "command execution failed.");
    }

    File_MaybeOverwrite(L, generated, buffer, len);
    return 0;
}

bool Pour_GenerateTarget(Target* target, genmode_t mode)
{
    lua_State* L = target->L;

    lua_pushlightuserdata(L, target);
    lua_pushinteger(L, mode);
    lua_pushcclosure(L, cmake_generate, 2);
    lua_setfield(L, target->globalsTableIdx, "cmake_generate");

    if (!File_Exists(L, target->buildDir))
        File_TryCreateDirectory(L, target->buildDir);

    if (!Script_LoadFunctions(L, target->globalsTableIdx))
        return false;

    setGlobals(target);
    if (!Script_DoFunction(L, target->luaScriptDir, target->buildDir, target->generateFn))
        return false;

    return true;
}

bool Pour_BuildTarget(Target* target, bool cleanFirst)
{
    lua_State* L = target->L;

    if (!Script_LoadFunctions(L, target->globalsTableIdx))
        return false;

    setGlobals(target);

    lua_pushboolean(L, cleanFirst);
    lua_setfield(L, target->globalsTableIdx, "CMAKE_FORCE_REBUILD");

    return Script_DoFunction(L, target->luaScriptDir, target->buildDir, target->buildFn);
}

/********************************************************************************************************************/

bool Pour_Build(lua_State* L, const char* sourceDir, const char* targetName, buildmode_t mode)
{
    int n = lua_gettop(L);
    luaL_checkstack(L, 1000, NULL);

    Target target;
    if (!Pour_LoadTarget(L, &target, sourceDir, targetName)) {
        lua_settop(L, n);
        return false;
    }

    genmode_t genmode = GEN_NORMAL;
    if (mode == BUILD_GENERATE_ONLY)
        genmode = GEN_FORCE;
    else if (mode == BUILD_GENERATE_ONLY_FORCE || mode == BUILD_REBUILD)
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
        case BUILD_REBUILD:
            if (!Pour_BuildTarget(&target, mode == BUILD_REBUILD)) {
                lua_settop(L, n);
                return false;
            }
            break;

        case BUILD_GENERATE_AND_OPEN: {
            const char* CMakeCache_txt = lua_pushfstring(L, "%s/%s", target.buildDir, "CMakeCache.txt");
            if (!File_Exists(L, CMakeCache_txt)) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: file \"%s\" was not found.\n", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            const char* data = File_PushContents(L, CMakeCache_txt, NULL);

            static const char param[] = "CMAKE_PROJECT_NAME:STATIC=";
            const char* p = strstr(data, param);
            if (!p) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: project name was not found in \"%s\".\n", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            p += sizeof(param) - 1;
            const char* end = strpbrk(p, "\r\n");
            if (!end || end == p) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: can't extract project name from \"%s\".\n", CMakeCache_txt);
                lua_settop(L, n);
                return false;
            }

            lua_pushstring(L, target.buildDir);
            lua_pushliteral(L, "/");
            lua_pushlstring(L, p, (size_t)(end - p));
            lua_pushliteral(L, ".sln");
            lua_concat(L, 4);
            const char* slnFile = lua_tostring(L, -1);
            if (File_Exists(L, slnFile)) {
                File_ShellOpen(L, slnFile);
                break;
            }

            Con_PrintF(L, COLOR_ERROR, "ERROR: file not found: \"%s\".\n", slnFile);
            lua_settop(L, n);
            return false;
        }
    }

    lua_settop(L, n);
    return true;
}
