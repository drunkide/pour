#ifndef POUR_POUR_H
#define POUR_POUR_H

#include <common/exec.h>

extern char PACKAGE_DIR;

bool Pour_Run(lua_State* L, const char* package, const char* chdir, int argc, char** argv, runmode_t mode);
bool Pour_ExecScript(lua_State* L, const char* script, const char* chdir, int argc, char** argv);
bool Pour_Install(lua_State* L, const char* package);

bool Pour_Main(lua_State* L, int argc, char** argv);

#endif
