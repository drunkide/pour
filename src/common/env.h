#ifndef COMMON_ENV_H
#define COMMON_ENV_H

#include <common/common.h>

const char* Env_PushGet(lua_State* L, const char* variable);
void Env_Set(lua_State* L, const char* variable, const char* value);

void Env_PrependPath(lua_State* L, const char* path);

#endif
