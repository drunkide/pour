#ifndef COMMON_EXEC_H
#define COMMON_EXEC_H

#include <common/common.h>

void Exec_Init(void);
void Exec_Terminate(void);

bool Exec_Command(const char* const* argv, int argc);
bool Exec_CommandV(const char* command, const char* const* argv, int argc);

#endif
