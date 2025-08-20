#ifndef COMMON_SCRIPT_H
#define COMMON_SCRIPT_H

#include <common/common.h>

typedef bool (*PFNMainProc)(int argc, char** argv);

extern lua_State* gL;

bool Script_DoFile(const char* name);
int Script_RunVM(int argc, char** argv, PFNMainProc pfnMain);

#endif
