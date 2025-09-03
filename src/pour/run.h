#ifndef POUR_RUN_H
#define POUR_RUN_H

#include <pour/pour.h>

bool Pour_Run(lua_State* L, const char* package, const char* chdir, int argc, char** argv, runmode_t mode);

#endif
