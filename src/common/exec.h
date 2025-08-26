#ifndef COMMON_EXEC_H
#define COMMON_EXEC_H

#include <common/common.h>

extern bool g_dont_print_commands;

void Exec_Init(void);
void Exec_Terminate(void);

bool Exec_Command(const char* const* argv, int argc, const char* chdir);
bool Exec_CommandV(const char* command, const char* const* argv, int argc, const char* chdir, bool wait);

#endif
