#ifndef COMMON_EXEC_H
#define COMMON_EXEC_H

#include <common/common.h>

typedef enum runmode_t {
    RUN_WAIT = 0,
    RUN_DONT_WAIT,
    RUN_DONT_WAIT_NO_CONSOLE,
} runmode_t;

extern bool g_dont_print_commands;

void Exec_Init(lua_State* L);
void Exec_Terminate(void);

bool Exec_Command(lua_State* L, const char* const* argv, int argc, const char* chdir);
bool Exec_CommandV(lua_State* L, const char* command, const char* const* argv, int argc,
    const char* chdir, runmode_t mode);

#endif
