#include <pour/package.h>
#include <pour/install.h>
#include <pour/script.h>
#include <common/console.h>
#include <common/env.h>
#include <common/dirs.h>
#include <common/file.h>
#include <common/script.h>
#include <string.h>

#define DEFAULT_EXECUTABLE_ID "_default_"

char PACKAGE_DIR;

/********************************************************************************************************************/

static void getGlobal(Package* pkg, const char* name)
{
    lua_State* L = pkg->L;
    lua_pushstring(L, name);
    lua_rawget(L, pkg->globalsTable);
}

static bool getBoolean(Package* pkg, const char* name)
{
    lua_State* L = pkg->L;
    getGlobal(pkg, name);
    bool value = lua_isboolean(L, -1) && lua_toboolean(L, -1);
    lua_pop(L, 1);
    return value;
}

static const char* getString(Package* pkg, const char* name)
{
    lua_State* L = pkg->L;
    getGlobal(pkg, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

static const char* getExecutable(Package* pkg, const char* name)
{
    lua_State* L = pkg->L;
    getGlobal(pkg, "EXECUTABLE");
    if (lua_isnoneornil(L, -1))
        return NULL;
    lua_getfield(L, -1, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

/********************************************************************************************************************/

static bool loadPackageConfig(Package* pkg)
{
    lua_State* L = pkg->L;
    char script[DIR_MAX];

    strcpy(script, g_packagesDir); /* FIXME: possible overflow */
    Dir_AppendPath(script, pkg->name); /* FIXME: possible overflow */
    strcat(script, ".lua"); /* FIXME: possible overflow */
    if (!File_Exists(L, script)) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unknown package '%s'.\n", pkg->name);
        return false;
    }

    if (!Script_DoFile(L, script, NULL, pkg->globalsTable))
        return false;

    pkg->TARGET_DIR = getString(pkg, "TARGET_DIR");
    pkg->SOURCE_URL = getString(pkg, "SOURCE_URL");
    pkg->CHECK_FILE = getString(pkg, "CHECK_FILE");
    pkg->INVOKE_LUA = getString(pkg, "INVOKE_LUA");
    pkg->DEFAULT_EXECUTABLE = getExecutable(pkg, DEFAULT_EXECUTABLE_ID);
    pkg->ADJUST_ARG = getBoolean(pkg, "ADJUST_ARG");

    if (!pkg->TARGET_DIR) {
        Con_PrintF(L, COLOR_ERROR,
            "ERROR: package '%s' is not available for current environment.\n", pkg->name);
        return false;
    }

  #ifdef _WIN32
    if (pkg->TARGET_DIR) {
        size_t len = strlen(pkg->TARGET_DIR) + 1;
        char* dir = (char*)lua_newuserdatauv(L, len, 0);
        memcpy(dir, pkg->TARGET_DIR, len);
        Dir_FromNativeSeparators(dir);
        pkg->TARGET_DIR = dir;
    }
  #endif

    lua_rawgetp(L, LUA_REGISTRYINDEX, &PACKAGE_DIR);
    lua_pushstring(L, pkg->TARGET_DIR);
    lua_setfield(L, -2, pkg->name);
    lua_pop(L, 1);

    getGlobal(pkg, "EXTRA_PATH");
    if (lua_isnoneornil(L, -1))
        lua_pop(L, 1);
    else {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            size_t strLen;
            const char* str = lua_tolstring(L, -1, &strLen);

          #ifdef _WIN32
            ++strLen;
            char* buf = (char*)lua_newuserdatauv(L, strLen, 0);
            memcpy(buf, str, strLen);
            Dir_ToNativeSeparators(buf);
            str = buf;
            lua_replace(L, -2);
          #else
            DONT_WARN_UNUSED(strLen);
          #endif

            Env_PrependPath(L, str);
            lua_pop(L, 1);
        }
    }

    getGlobal(pkg, "EXTRA_VARS");
    if (lua_isnoneornil(L, -1))
        lua_pop(L, 1);
    else {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const char* name = lua_tostring(L, -2);
            const char* value = lua_tostring(L, -1);
            Env_Set(L, name, value);
            lua_pop(L, 1);
        }
    }

    getGlobal(pkg, "EXTRA_DEPS");
    if (lua_isnoneornil(L, -1))
        lua_pop(L, 1);
    else {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const char* dep = lua_tostring(L, -1);
            if (!Pour_Install(L, dep, false)) {
                Con_PrintF(L, COLOR_ERROR,
                    "ERROR: unable to install dependency '%s' for package '%s'.\n", dep, pkg->name);
                return false;
            }
            lua_pop(L, 1);
        }
    }

    return true;
}

/********************************************************************************************************************/

const char* Pour_GetPackageExecutable(Package* pkg, const char* executable)
{
    lua_State* L = pkg->L;
    const char* exe;

    if (executable) {
        exe = getExecutable(pkg, executable);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: there is no executable '%s' in package '%s'.\n",
                executable, pkg->name);
            return NULL;
        }
    } else {
        if (!pkg->DEFAULT_EXECUTABLE) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: there is no default executable in package '%s'.\n",
                pkg->name);
            return NULL;
        }
        exe = getExecutable(pkg, pkg->DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                pkg->name, pkg->DEFAULT_EXECUTABLE);
            return NULL;
        }
    }

    return exe;
}

/********************************************************************************************************************/

static void adjustPath(char* dst)
{
    if ((dst[0] >= 'A' && dst[0] <= 'Z') && dst[1] == ':')
        dst[0] += ('a' - 'A');

    if ((dst[0] >= 'a' && dst[0] <= 'z') && dst[1] == ':') {
        dst[1] = dst[0];
        dst[0] = '/';
        Dir_FromNativeSeparators(dst + 2);
    }
}

void Pour_AdjustCommandLineArguments(Package* pkg, int argc, char** argv)
{
    lua_State* L = pkg->L;

    if (!pkg->ADJUST_ARG)
        return;

    const char* prev = NULL;
    for (int i = 1; i < argc; prev = argv[i], i++) {
        if (argv[i][0] != '-') {
            if (strpbrk(argv[i], "./\\:") != NULL || (prev && !strcmp(prev, "-o"))) {
                size_t len = strlen(argv[i]) + 1;
                char* path = (char*)lua_newuserdatauv(L, len + DIR_MAX, 0);
                memcpy(path, argv[i], len);
                Dir_MakeAbsolutePath(L, path, len + DIR_MAX);
                adjustPath(path);
                argv[i] = path;
            }
        } else {
            if (argv[i][1] == 'I')
                adjustPath(argv[i] + 2);
            else if (argv[i][1] == 'L')
                adjustPath(argv[i] + 2);
        }
    }
}

/********************************************************************************************************************/

bool Pour_EnsurePackageConfigured(Package* pkg)
{
    lua_State* L = pkg->L;

    getGlobal(pkg, "POST_FETCH");
    if (lua_isnoneornil(L, -1))
        return true;
    int idx = lua_gettop(L);

    lua_pushfstring(L, "%s/.pour-configured", pkg->TARGET_DIR);
    const char* checkFile = lua_tostring(L, -1);
    if (File_Exists(L, checkFile))
        return true;

    bool result;
    if (lua_isfunction(L, idx))
        result = Script_DoFunction(L, NULL, pkg->TARGET_DIR, idx);
    else if (lua_isstring(L, idx)) {
        lua_newtable(L);
        result = Script_DoFile(L, lua_tostring(L, idx), NULL, lua_gettop(L));
    } else {
        Con_PrintF(L, COLOR_ERROR, "ERROR: '%s': %s should be string or table.\n", pkg->name, "POST_FETCH");
        return false;
    }

    if (!result) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unable to configure package '%s'.\n", pkg->name);
        return false;
    }

    File_Overwrite(L, checkFile, "", 0);
    return true;
}

/********************************************************************************************************************/

bool Pour_EnsurePackageInstalled(Package* pkg)
{
    lua_State* L = pkg->L;

    if (!loadPackageConfig(pkg))
        return false;

    if (pkg->CHECK_FILE) {
        if (File_Exists(L, pkg->CHECK_FILE))
            return Pour_EnsurePackageConfigured(pkg);
    } else if (pkg->INVOKE_LUA) {
        if (File_Exists(L, pkg->INVOKE_LUA))
            return Pour_EnsurePackageConfigured(pkg);
    } else if (pkg->DEFAULT_EXECUTABLE) {
        const char* exe = getExecutable(pkg, pkg->DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                pkg->name, pkg->DEFAULT_EXECUTABLE);
            return false;
        }
        if (File_Exists(L, exe))
            return Pour_EnsurePackageConfigured(pkg);
    } else {
        Con_PrintF(L, COLOR_ERROR, "ERROR: missing CHECK_FILE for package '%s'.\n", pkg->name);
        return false;
    }

    if (pkg->SOURCE_URL) {
        const char* argv[4];
        argv[0] = "git";
        argv[1] = "clone";
        argv[2] = pkg->SOURCE_URL;
        argv[3] = pkg->TARGET_DIR;
        if (!Exec_Command(L, argv, 4, NULL)) {
            Con_PrintF(L, COLOR_ERROR, "ERROR: unable to download package '%s'.\n", pkg->name);
            return false;
        }
        return Pour_EnsurePackageConfigured(pkg);
    }

    Con_PrintF(L, COLOR_ERROR, "ERROR: missing SOURCE_URL for package '%s'.\n", pkg->name);
    return false;
}

/********************************************************************************************************************/

void Pour_InitPackage(lua_State* L, Package* pkg, const char* name)
{
    luaL_checkstack(L, 100, NULL);
    pkg->L = L;
    pkg->name = name;
    pkg->globalsTable = Pour_PushNewGlobalsTable(L);
}
