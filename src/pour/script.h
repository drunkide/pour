#ifndef POUR_SCRIPT_H
#define POUR_SCRIPT_H

#include <pour/pour.h>

extern char ARG;

bool Pour_ExecScript(lua_State* L, const char* script, const char* chdir, int argc, char** argv);
void Pour_InvokeScript(lua_State* L, const char* script);

#endif
