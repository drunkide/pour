#ifndef COMMON_SCRIPT_H
#define COMMON_SCRIPT_H

#include <common/common.h>

typedef bool (*PFNMainProc)(lua_State* L, int argc, char** argv);

bool Script_IsAbnormalTermination(lua_State* L);
void Script_GetString(lua_State* L, int index, char* buf, size_t bufSize, const char* error);
const char* Script_GetCurrentScriptDir(lua_State* L);

void Script_Interrupt(void);
bool Script_DoFile(lua_State* L, const char* name, const char* chdir, int globalsTableIdx);
bool Script_DoFunction(lua_State* L, const char* scriptDir, const char* chdir, int functionIdx);

bool Script_LoadFunctions(lua_State* L, int globalsTableIdx);

int Script_RunVM(int argc, char** argv, PFNMainProc pfnMain);

#endif
