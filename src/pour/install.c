#include <pour/install.h>
#include <pour/package.h>
#include <pour/script.h>

bool Pour_Install(lua_State* L, const char* package, bool skipInvoke)
{
    int n = lua_gettop(L);
    Package pkg;

    Pour_InitPackage(L, &pkg, package);

    if (!Pour_EnsurePackageInstalled(&pkg)) {
        lua_settop(L, n);
        return false;
    }

    if (!skipInvoke && pkg.INVOKE_LUA)
        Pour_InvokeScript(L, pkg.INVOKE_LUA);

    lua_settop(L, n);
    return true;
}
