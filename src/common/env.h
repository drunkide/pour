#ifndef COMMON_ENV_H
#define COMMON_ENV_H

#include <common/common.h>

void Env_Set(const char* variable, const char* value);
void Env_PrependPath(const char* path);

#endif
