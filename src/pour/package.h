#ifndef POUR_PACKAGE_H
#define POUR_PACKAGE_H

#include <common/common.h>

STRUCT(Package) {
    lua_State* L;
    int globalsTable;
    const char* name;
    const char* TARGET_DIR;
    const char* SOURCE_URL;
    const char* CHECK_FILE;
    const char* INVOKE_LUA;
    const char* DEFAULT_EXECUTABLE;
    bool ADJUST_ARG;
};

extern char PACKAGE_DIR;

const char* Pour_GetPackageExecutable(Package* pkg, const char* executable);
void Pour_AdjustCommandLineArguments(Package* pkg, int argc, char** argv);

bool Pour_EnsurePackageConfigured(Package* pkg);
bool Pour_EnsurePackageInstalled(Package* pkg);

void Pour_InitPackage(lua_State* L, Package* pkg, const char* name);

#endif
