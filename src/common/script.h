#ifndef COMMON_SCRIPT_H
#define COMMON_SCRIPT_H

#include <common/common.h>

typedef bool (*PFNMainProc)(lua_State* L, int argc, char** argv);

bool Script_DoFile(lua_State* L, const char* name, const char* chdir, int globalsTableIdx);
int Script_RunVM(int argc, char** argv, PFNMainProc pfnMain);

#endif
