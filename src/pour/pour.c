#include <pour/pour.h>
#include <common/script.h>
#include <common/dirs.h>
#include <common/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************************************************************/

static const char* TARGET_DIR;
static const char* SOURCE_URL;
static const char* CHECK_FILE;
static const char* DEFAULT_EXECUTABLE;

static const char* getGlobal(const char* name)
{
    lua_State* L = gL;
    lua_getglobal(L, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

static const char* getExecutable(const char* name)
{
    lua_State* L = gL;
    lua_getglobal(L, "EXECUTABLE");
    if (lua_isnoneornil(L, -1))
        return NULL;
    lua_getfield(L, -1, name);
    return (!lua_isnoneornil(L, -1) ? lua_tostring(L, -1) : NULL);
}

static bool loadPackageConfig(const char* package)
{
    char script[DIR_MAX];

    strcpy(script, g_packagesDir);
    Dir_AppendPath(script, package);
    strcat(script, ".lua");
    if (!File_Exists(script)) {
        fprintf(stderr, "error: unknown package '%s'.\n", package);
        return false;
    }

    if (!Script_DoFile(script))
        return false;

    TARGET_DIR = getGlobal("TARGET_DIR");
    SOURCE_URL = getGlobal("SOURCE_URL");
    CHECK_FILE = getGlobal("CHECK_FILE");
    DEFAULT_EXECUTABLE = getExecutable("_default_");

    if (!TARGET_DIR) {
        fprintf(stderr, "error: package '%s' is not available for current environment.\n", package);
        return false;
    }

    return true;
}

/********************************************************************************************************************/

bool Pour_Run(const char* package, int argc, char** argv)
{
    /* FIXME */
    (void)package;
    (void)argc;
    (void)argv;
    return false;
}

/********************************************************************************************************************/

bool Pour_Install(const char* package)
{
    lua_State* L = gL;
    int n = lua_gettop(L);

    if (!loadPackageConfig(package)) {
        lua_settop(L, n);
        return false;
    }

    if (CHECK_FILE) {
        char file[DIR_MAX];
        strcpy(file, TARGET_DIR);
        Dir_AppendPath(file, CHECK_FILE);
        if (File_Exists(file)) {
            lua_settop(L, n);
            return true;
        }
    } else if (DEFAULT_EXECUTABLE) {
        const char* exe = getExecutable(DEFAULT_EXECUTABLE);
        if (!exe) {
            fprintf(stderr,
                "error: configuration for package '%s' is corrupt (missing executable '%s').\n",
                package, exe);
            lua_settop(L, n);
            return false;
        }
        if (File_Exists(exe)) {
            lua_settop(L, n);
            return true;
        }
    } else {
        fprintf(stderr, "error: missing CHECK_FILE for package '%s'.\n", package);
        lua_settop(L, n);
        return false;
    }

    if (SOURCE_URL) {
        char command[2048];
        sprintf(command, "git clone \"%s\" \"%s\"", SOURCE_URL, TARGET_DIR);
        printf("# %s\n", command);
        if (system(command) != 0) {
           fprintf(stderr, "error: unable to download package '%s'.\n", package);
           lua_settop(L, n);
           return false;
        }
        lua_settop(L, n);
        return true;
    }

    fprintf(stderr, "error: missing SOURCE_URL for package '%s'.\n", package);
    lua_settop(L, n);
    return false;
}

/********************************************************************************************************************/

STRUCT(Package) {
    Package* next;
    const char* name;
};

bool Pour_Main(int argc, char** argv)
{
    if (argc > 1 && !strcmp(argv[1], "--run")) {
        if (argc < 3) {
            fprintf(stderr, "error: missing parameter name after '--run'.\n");
            return false;
        }
        return Pour_Run(argv[2], argc - 2, argv + 2);
    }

    Package* firstPackage = NULL;
    Package* lastPackage = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            Package* p = (Package*)lua_newuserdata(gL, sizeof(Package));
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
            printf("usage: pour [options] package...\n");
            printf("    or pour --run package [options]\n");
            return false;
        } else {
            fprintf(stderr, "error: invalid command line argument \"%s\".\n", argv[i]);
            return false;
        }
    }

    if (!firstPackage) {
        fprintf(stderr, "error: missing package name on the command line.\n");
        return false;
    }

    for (Package* p = firstPackage; p; p = p->next) {
        if (!Pour_Install(p->name))
            return false;
    }

    return true;
}
