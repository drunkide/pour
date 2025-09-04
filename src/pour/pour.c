#include <pour/pour.h>
#include <pour/pour_lua.h>
#include <pour/script.h>
#include <pour/run.h>
#include <pour/install.h>
#include <pour/build.h>
#include <common/console.h>
#include <common/env.h>
#include <string.h>

const char* g_pourExecutable;
bool g_verbose;

/********************************************************************************************************************/

STRUCT(PackageName) {
    PackageName* next;
    const char* name;
};

bool Pour_Main(lua_State* L, int argc, char** argv)
{
    const char* chdir = NULL;
    buildmode_t buildmode;
    int n;

    Env_Set(L, "POUR_EXECUTABLE", argv[0]);
    g_pourExecutable = argv[0];

    for (n = 1; n < argc; n++) {
        if (!strcmp(argv[n], "--dont-print-commands"))
            g_dont_print_commands = true;
        else if (!strcmp(argv[n], "--verbose"))
            g_verbose = true;
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
        } else if (!strcmp(argv[n], "--generate")) {
            buildmode = BUILD_GENERATE_ONLY;
            goto build;
        } else if (!strcmp(argv[n], "--build")) {
            buildmode = BUILD_NORMAL;
          build:
            if (n + 1 >= argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: missing target name after '%s'.\n", argv[n]);
                return false;
            }
            const char* target = argv[++n];
            for (++n; n < argc; ++n) {
                if (!strcmp(argv[n], "--verbose"))
                    g_verbose = true;
                else if (!strcmp(argv[n], "--force")) {
                    if (buildmode == BUILD_GENERATE_ONLY)
                        buildmode = BUILD_GENERATE_ONLY_FORCE;
                    else if (buildmode == BUILD_NORMAL)
                        buildmode = BUILD_REBUILD;
                } else {
                    Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected command line argument \"%s\".\n", argv[n]);
                    return false;
                }
            }
            return Pour_Build(L, target, buildmode);
        } else if (!strcmp(argv[n], "--develop")) {
            if (n + 1 >= argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: missing target name after '%s'.\n", argv[n]);
                return false;
            }
            const char* target = argv[++n];
            if (++n < argc) {
                Con_PrintF(L, COLOR_ERROR, "ERROR: unexpected command line argument \"%s\".\n", argv[n]);
                return false;
            }
            return Pour_Build(L, target, BUILD_GENERATE_AND_OPEN);
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
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --run <package>[:<cmd>] [args...]\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --script <file> [args...]\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --generate <target> [--force]\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --build <target> [--force]\n");
            Con_Print(L, COLOR_DEFAULT, "    or pour [options] --develop <target>\n");
            Con_Print(L, COLOR_DEFAULT, "\n");
            Con_Print(L, COLOR_DEFAULT, "where commands are:\n");
            Con_Print(L, COLOR_DEFAULT, " --run <package>        run default command from the package.\n");
            Con_Print(L, COLOR_DEFAULT, " --run <package>:<cmd>  run specified command from the package.\n");
            Con_Print(L, COLOR_DEFAULT, " --script <file>        execute the specified Lua script.\n");
            Con_Print(L, COLOR_DEFAULT, " --generate <target>    generate project for the specified target.\n");
            Con_Print(L, COLOR_DEFAULT, " --build <target>       build project for the specified target.\n");
            Con_Print(L, COLOR_DEFAULT, " --develop <target>     open project for the specified target in IDE.\n");
            Con_Print(L, COLOR_DEFAULT, "\n");
            Con_Print(L, COLOR_DEFAULT, "where options are:\n");
            Con_Print(L, COLOR_DEFAULT, " --chdir <path>         set working directory before performing action.\n");
            Con_Print(L, COLOR_DEFAULT, " --dont-print-commands  avoid displaying commands to be executed.\n");
            Con_Print(L, COLOR_DEFAULT, " --verbose              be more verbose, if possible.\n");
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
