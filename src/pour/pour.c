#include <pour/pour.h>
#include <pour/pour_lua.h>
#include <common/script.h>
#include <common/console.h>
#include <common/env.h>
#include <common/dirs.h>
#include <common/file.h>
#include <common/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_EXECUTABLE_ID "_default_"

char PACKAGE_DIR;
char ARG;

/********************************************************************************************************************/

STRUCT(Package) {
    lua_State* L;
    int globalsTable;
    const char* TARGET_DIR;
    const char* SOURCE_URL;
    const char* CHECK_FILE;
    const char* INVOKE_LUA;
    const char* DEFAULT_EXECUTABLE;
    bool ADJUST_ARG;
};

static void getGlobal(Package* pkg, const char* name)
{
    lua_State* L = pkg->L;
    lua_pushvalue(L, pkg->globalsTable);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
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

static bool loadPackageConfig(Package* pkg, const char* package)
{
    lua_State* L = pkg->L;
    char script[DIR_MAX];

    strcpy(script, g_packagesDir);
    Dir_AppendPath(script, package);
    strcat(script, ".lua");
    if (!File_Exists(L, script)) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unknown package '%s'.\n", package);
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
        Con_PrintF(L, COLOR_ERROR, "ERROR: package '%s' is not available for current environment.\n", package);
        return false;
    }

    lua_rawgetp(L, LUA_REGISTRYINDEX, &PACKAGE_DIR);
    lua_pushstring(L, pkg->TARGET_DIR);
    lua_setfield(L, -2, package);
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

    return true;
}

static bool ensurePackageConfigured(Package* pkg, const char* package)
{
    lua_State* L = pkg->L;

    const char* POST_FETCH = getString(pkg, "POST_FETCH");
    if (!POST_FETCH)
        return true;

    lua_pushfstring(L, "%s/.pour-configured", pkg->TARGET_DIR);
    const char* checkFile = lua_tostring(L, -1);
    if (File_Exists(L, checkFile))
        return true;

    lua_newtable(L);
    if (!Script_DoFile(L, POST_FETCH, NULL, lua_gettop(L))) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: unable to configure package '%s'.\n", package);
        return false;
    }

    File_Overwrite(L, checkFile, "", 0);

    return true;
}

static bool ensurePackageInstalled(Package* pkg, const char* package)
{
    lua_State* L = pkg->L;

    if (!loadPackageConfig(pkg, package))
        return false;

    if (pkg->CHECK_FILE) {
        if (File_Exists(L, pkg->CHECK_FILE))
            return ensurePackageConfigured(pkg, package);
    } else if (pkg->INVOKE_LUA) {
        if (File_Exists(L, pkg->INVOKE_LUA))
            return ensurePackageConfigured(pkg, package);
    } else if (pkg->DEFAULT_EXECUTABLE) {
        const char* exe = getExecutable(pkg, pkg->DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                package, pkg->DEFAULT_EXECUTABLE);
            return false;
        }
        if (File_Exists(L, exe))
            return ensurePackageConfigured(pkg, package);
    } else {
        Con_PrintF(L, COLOR_ERROR, "ERROR: missing CHECK_FILE for package '%s'.\n", package);
        return false;
    }

    if (pkg->SOURCE_URL) {
        const char* argv[4];
        argv[0] = "git";
        argv[1] = "clone";
        argv[2] = pkg->SOURCE_URL;
        argv[3] = pkg->TARGET_DIR;
        if (!Exec_Command(L, argv, 4, NULL)) {
            Con_PrintF(L, COLOR_ERROR, "ERROR: unable to download package '%s'.\n", package);
            return false;
        }
        return ensurePackageConfigured(pkg, package);
    }

    Con_PrintF(L, COLOR_ERROR, "ERROR: missing SOURCE_URL for package '%s'.\n", package);
    return false;
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

bool Pour_Run(lua_State* L, const char* package, const char* chdir, int argc, char** argv, runmode_t mode)
{
    int n = lua_gettop(L);
    Package pkg;

    pkg.L = L;
    luaL_checkstack(L, 100, NULL);

    lua_newtable(L);
    pkg.globalsTable = lua_gettop(L);

    const char* executable = NULL;
    const char* colon = strchr(package, ':');
    if (colon) {
        size_t len = colon - package;
        char* buf = (char*)lua_newuserdatauv(L, len + 1, 0);
        memcpy(buf, package, len);
        buf[len] = 0;
        executable = colon + 1;
        package = buf;
    }

    if (!ensurePackageInstalled(&pkg, package)) {
      error:
        lua_settop(L, n);
        return false;
    }

    const char* exe;
    if (executable) {
        exe = getExecutable(&pkg, executable);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: there is no executable '%s' in package '%s'.\n",
                executable, package);
            goto error;
        }
    } else {
        if (!pkg.DEFAULT_EXECUTABLE) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: there is no default executable in package '%s'.\n",
                package);
            goto error;
        }
        exe = getExecutable(&pkg, pkg.DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(L, COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                package, pkg.DEFAULT_EXECUTABLE);
            goto error;
        }
    }

    if (pkg.ADJUST_ARG) {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] != '-') {
                if (strpbrk(argv[i], "./\\:") != NULL) {
                    char* path = (char*)lua_newuserdatauv(L, DIR_MAX, 0);
                    strcpy(path, argv[i]);
                    Dir_MakeAbsolutePath(L, path, DIR_MAX);
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

    if (!Exec_CommandV(L, exe, (const char* const*)argv, argc, chdir, mode))
        goto error;

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

bool Pour_Install(lua_State* L, const char* package, bool skipInvoke)
{
    int n = lua_gettop(L);
    Package pkg;

    pkg.L = L;

    lua_newtable(L);
    pkg.globalsTable = lua_gettop(L);

    if (!ensurePackageInstalled(&pkg, package)) {
        lua_settop(L, n);
        return false;
    }

    if (!skipInvoke && pkg.INVOKE_LUA)
        Pour_InvokeScript(L, pkg.INVOKE_LUA);

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

bool Pour_ExecScript(lua_State* L, const char* script, const char* chdir, int argc, char** argv)
{
    int n = lua_gettop(L);

    lua_newtable(L);
    int globalsTableIdx = lua_gettop(L);

    lua_createtable(L, argc, argc);
    for (int i = 1, idx = 1; i < argc; i++) {
        const char* p = strchr(argv[i], '=');
        if (p) {
            lua_pushlstring(L, argv[i], p - argv[i]);

            ++p;
            if (*p != '{')
                lua_pushstring(L, p);
            else {
                lua_pushfstring(L, "return %s", p);
                if (luaL_loadstring(L, lua_tostring(L, -1)) != 0)
                    luaL_error(L, "syntax error in expression: %s", p);
                lua_remove(L, -2);
                if (lua_pcall(L, 0, 1, 0))
                    luaL_error(L, "error evaluating expression: %s", p);
            }

            lua_rawset(L, -3);
            continue;
        }

        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, idx++);
    }

    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &ARG);
    lua_setfield(L, globalsTableIdx, "arg");

    if (!Script_DoFile(L, script, chdir, globalsTableIdx)) {
        lua_settop(L, n);
        return false;
    }

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

void Pour_InvokeScript(lua_State* L, const char* script)
{
    int n = lua_gettop(L);

    size_t len = strlen(script) + 1;
    char* dir = (char*)lua_newuserdata(L, len);
    memcpy(dir, script, len);
    if (!Dir_RemoveLastPath(dir))
        dir = NULL;

    lua_newtable(L);
    lua_rawgetp(L, LUA_REGISTRYINDEX, &ARG);
    lua_setfield(L, -2, "arg");
    int globalsTableIdx = lua_gettop(L);

    if (!Script_DoFile(L, script, dir, globalsTableIdx))
        luaL_error(L, "execution of script \"%s\" failed.", script);

    lua_settop(L, n);
}

/********************************************************************************************************************/

STRUCT(PackageName) {
    PackageName* next;
    const char* name;
};

bool Pour_Main(lua_State* L, int argc, char** argv)
{
    const char* chdir = NULL;
    int n;

    Env_Set(L, "POUR_EXECUTABLE", argv[0]);

    for (n = 1; n < argc; n++) {
        if (!strcmp(argv[n], "--dont-print-commands"))
            g_dont_print_commands = true;
        else if (!strcmp(argv[n], "--chdir")) {
            if (n + 1 >= argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: missing directory name after '%s'.\n", argv[n]);
                return false;
            }
            chdir = argv[++n];
        } else if (!strcmp(argv[n], "--run")) {
            if (n + 1 >= argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: missing package name after '%s'.\n", argv[n]);
                return false;
            }
            ++n;
            return Pour_Run(L, argv[n], chdir, argc - n, argv + n, RUN_WAIT);
        } else if (!strcmp(argv[n], "--script")) {
            if (n + 1 >= argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: missing script name after '%s'.\n", argv[n]);
                return false;
            }
            ++n;
            return Pour_ExecScript(L, argv[n], chdir, argc - n, argv + n);
        } else if (!strcmp(argv[n], "-h") || !strcmp(argv[n], "--help"))
            goto help;
        else
            break;
    }

    if (chdir)
        Con_PrintF(L, COLOR_WARNING, "WARNING: unexpected command line argument \"%s\", ignored.\n", "--chdir");

    PackageName* firstPackage = NULL;
    PackageName* lastPackage = NULL;

    for (int i = n; i < argc; i++) {
        if (argv[i][0] != '-') {
            PackageName* p = (PackageName*)lua_newuserdatauv(L, sizeof(PackageName), 0);
            p->next = NULL;
            p->name = argv[i];
            if (!firstPackage)
                firstPackage = p;
            else
                lastPackage->next = p;
            lastPackage = p;
            continue;
        }

        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
          help:
            Con_Print(L, COLOR_DEFAULT, "usage: pour [options] package...\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --run package [args...]\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --script file [args...]\n");
            Con_Print(L, COLOR_DEFAULT, "\n");
            Con_Print(L, COLOR_DEFAULT, "where options are:\n");
            Con_Print(L, COLOR_DEFAULT, " --chdir <path>         set working directory before performing action.\n");
            Con_Print(L, COLOR_DEFAULT, " --dont-print-commands  avoid displaying commands to be executed.\n");
            Con_Print(L, COLOR_DEFAULT, "\n");
            return false;
        } else {
            Con_PrintF(L, COLOR_ERROR, "ERROR: invalid command line argument \"%s\".\n", argv[i]);
            return false;
        }
    }

    if (!firstPackage) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: missing package name on the command line.\n");
        return false;
    }

    for (PackageName* p = firstPackage; p; p = p->next) {
        if (!Pour_Install(L, p->name, true))
            return false;
    }

    return true;
}
