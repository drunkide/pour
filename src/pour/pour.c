#include <pour/pour.h>
#include <pour/pour_lua.h>
#include <pour/script.h>
#include <pour/run.h>
#include <pour/install.h>
#include <common/console.h>
#include <common/env.h>
#include <string.h>

const char* g_pourExecutable;

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
    g_pourExecutable = argv[0];

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
