#ifndef POUR_POUR_H
#define POUR_POUR_H

#include <common/exec.h>

extern const char* g_pourExecutable;
extern bool g_verbose;

bool Pour_Main(lua_State* L, int argc, char** argv);

#endif
