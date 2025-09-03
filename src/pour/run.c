#include <pour/run.h>
#include <pour/package.h>
#include <string.h>

bool Pour_Run(lua_State* L, const char* package, const char* chdir, int argc, char** argv, runmode_t mode)
{
    int n = lua_gettop(L);
    Package pkg;

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

    Pour_InitPackage(L, &pkg, package);

    if (!Pour_EnsurePackageInstalled(&pkg)) {
      error:
        lua_settop(L, n);
        return false;
    }

    const char* exe = Pour_GetPackageExecutable(&pkg, executable);
    if (!exe)
        goto error;

    Pour_AdjustCommandLineArguments(&pkg, argc, argv);

    if (!Exec_CommandV(L, exe, (const char* const*)argv, argc, chdir, mode))
        goto error;

    lua_settop(L, n);
    return true;
}
