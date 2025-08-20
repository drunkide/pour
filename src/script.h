#ifndef POUR_SCRIPT_H
#define POUR_SCRIPT_H

#include <pour.h>

extern lua_State* gL;

int Script_Call(lua_State* L, int narg, int nres);
int Script_CheckError(lua_State* L, int status);

int Script_DoFile(const char* name);

#endif
