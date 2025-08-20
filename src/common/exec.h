#ifndef COMMON_EXEC_H
#define COMMON_EXEC_H

#include <common/common.h>

bool Exec_Command(const char* const* argv, int argc);
bool Exec_CommandV(const char* command, const char* const* argv, int argc);

#endif
