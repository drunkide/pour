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
#include <malloc.h>

#define DEFAULT_EXECUTABLE_ID "_default_"

/********************************************************************************************************************/

STRUCT(Package) {
    int globalsTable;
    const char* TARGET_DIR;
    const char* SOURCE_URL;
    const char* CHECK_FILE;
    const char* DEFAULT_EXECUTABLE;
};

static void getGlobal(Package* pkg, const char* name)
{
    lua_State* L = gL;
    lua_pushvalue(L, pkg->globalsTable);
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
}

static const char* getString(Package* pkg, const char* name)
{
    lua_State* L = gL;
    getGlobal(pkg, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

static const char* getExecutable(Package* pkg, const char* name)
{
    lua_State* L = gL;
    getGlobal(pkg, "EXECUTABLE");
    if (lua_isnoneornil(L, -1))
        return NULL;
    lua_getfield(L, -1, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

static bool loadPackageConfig(Package* pkg, const char* package)
{
    lua_State* L = gL;
    char script[DIR_MAX];

    strcpy(script, g_packagesDir);
    Dir_AppendPath(script, package);
    strcat(script, ".lua");
    if (!File_Exists(script)) {
        Con_PrintF(COLOR_ERROR, "ERROR: unknown package '%s'.\n", package);
        return false;
    }

    if (!Script_DoFile(script, pkg->globalsTable))
        return false;

    pkg->TARGET_DIR = getString(pkg, "TARGET_DIR");
    pkg->SOURCE_URL = getString(pkg, "SOURCE_URL");
    pkg->CHECK_FILE = getString(pkg, "CHECK_FILE");
    pkg->DEFAULT_EXECUTABLE = getExecutable(pkg, DEFAULT_EXECUTABLE_ID);

    if (!pkg->TARGET_DIR) {
        Con_PrintF(COLOR_ERROR, "ERROR: package '%s' is not available for current environment.\n", package);
        return false;
    }

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
            char* buf = alloca(strLen);
            memcpy(buf, str, strLen);
            Dir_ToNativeSeparators(buf);
            str = buf;
          #else
            (void)strLen;
          #endif

            Env_PrependPath(str);
            lua_pop(L, 1);
        }
    }

    return true;
}

static bool ensurePackageConfigured(Package* pkg, const char* package)
{
    lua_State* L = gL;

    const char* POST_FETCH = getString(pkg, "POST_FETCH");
    if (!POST_FETCH)
        return true;

    lua_pushfstring(L, "%s/.pour-configured", pkg->TARGET_DIR);
    const char* checkFile = lua_tostring(L, -1);
    if (File_Exists(checkFile))
        return true;

    lua_newtable(L);
    if (!Script_DoFile(POST_FETCH, lua_gettop(L))) {
        Con_PrintF(COLOR_ERROR, "ERROR: unable to configure package '%s'.\n", package);
        return false;
    }

    File_Write(checkFile, "", 0);

    return true;
}

static bool ensurePackageInstalled(Package* pkg, const char* package)
{
    if (!loadPackageConfig(pkg, package))
        return false;

    if (pkg->CHECK_FILE) {
        if (File_Exists(pkg->CHECK_FILE))
            return ensurePackageConfigured(pkg, package);
    } else if (pkg->DEFAULT_EXECUTABLE) {
        const char* exe = getExecutable(pkg, pkg->DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                package, pkg->DEFAULT_EXECUTABLE);
            return false;
        }
        if (File_Exists(exe))
            return ensurePackageConfigured(pkg, package);
    } else {
        Con_PrintF(COLOR_ERROR, "ERROR: missing CHECK_FILE for package '%s'.\n", package);
        return false;
    }

    if (pkg->SOURCE_URL) {
        const char* argv[4];
        argv[0] = "git";
        argv[1] = "clone";
        argv[2] = pkg->SOURCE_URL;
        argv[3] = pkg->TARGET_DIR;
        if (!Exec_Command(argv, 4)) {
            Con_PrintF(COLOR_ERROR, "ERROR: unable to download package '%s'.\n", package);
            return false;
        }
        return ensurePackageConfigured(pkg, package);
    }

    Con_PrintF(COLOR_ERROR, "ERROR: missing SOURCE_URL for package '%s'.\n", package);
    return false;
}

/********************************************************************************************************************/

bool Pour_Run(const char* package, int argc, char** argv)
{
    lua_State* L = gL;
    int n = lua_gettop(L);
    Package pkg;

    luaL_checkstack(L, 100, NULL);

    lua_newtable(L);
    pkg.globalsTable = lua_gettop(L);

    const char* executable = NULL;
    const char* colon = strchr(package, ':');
    if (colon) {
        size_t len = colon - package;
        char* buf = (char*)alloca(len + 1);
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
            Con_PrintF(COLOR_ERROR, "ERROR: there is no executable '%s' in package '%s'.\n", executable, package);
            goto error;
        }
    } else {
        if (!pkg.DEFAULT_EXECUTABLE) {
            Con_PrintF(COLOR_ERROR, "ERROR: there is no default executable in package '%s'.\n", package);
            goto error;
        }
        exe = getExecutable(&pkg, pkg.DEFAULT_EXECUTABLE);
        if (!exe) {
            Con_PrintF(COLOR_ERROR,
                "ERROR: configuration for package '%s' is corrupt (missing executable '%s').\n",
                package, pkg.DEFAULT_EXECUTABLE);
            goto error;
        }
    }

    if (!Exec_CommandV(exe, (const char* const*)argv, argc))
        goto error;

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

bool Pour_ExecScript(const char* script, int argc, char** argv)
{
    lua_State* L = gL;
    int n = lua_gettop(L);

    (void)argc;
    (void)argv;

    if (!Script_DoFile(script, 0)) {
        lua_settop(L, n);
        return false;
    }

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

bool Pour_Install(const char* package)
{
    lua_State* L = gL;
    int n = lua_gettop(L);
    Package pkg;

    lua_newtable(L);
    pkg.globalsTable = lua_gettop(L);

    if (!ensurePackageInstalled(&pkg, package)) {
        lua_settop(L, n);
        return false;
    }

    lua_settop(L, n);
    return true;
}

/********************************************************************************************************************/

STRUCT(PackageName) {
    PackageName* next;
    const char* name;
};

bool Pour_Main(int argc, char** argv)
{
    Env_Set("POUR_EXECUTABLE", argv[0]);

    if (argc > 1 && !strcmp(argv[1], "--run")) {
        if (argc < 3) {
            Con_PrintF(COLOR_ERROR, "ERROR: missing package name after '%s'.\n", argv[1]);
            return false;
        }
        return Pour_Run(argv[2], argc - 2, argv + 2);
    }

    if (argc > 1 && !strcmp(argv[1], "--script")) {
        if (argc < 3) {
            Con_PrintF(COLOR_ERROR, "ERROR: missing script name after '%s'.\n", argv[1]);
            return false;
        }
        return Pour_ExecScript(argv[2], argc - 2, argv + 2);
    }

    PackageName* firstPackage = NULL;
    PackageName* lastPackage = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            PackageName* p = (PackageName*)lua_newuserdata(gL, sizeof(PackageName));
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
            Con_PrintF(COLOR_DEFAULT, "usage: pour [options] package...\n");
            Con_PrintF(COLOR_DEFAULT, "    or pour --run package [options]\n");
            return false;
        } else {
            Con_PrintF(COLOR_ERROR, "ERROR: invalid command line argument \"%s\".\n", argv[i]);
            return false;
        }
    }

    if (!firstPackage) {
        Con_PrintF(COLOR_ERROR, "ERROR: missing package name on the command line.\n");
        return false;
    }

    for (PackageName* p = firstPackage; p; p = p->next) {
        if (!Pour_Install(p->name))
            return false;
    }

    return true;
}
