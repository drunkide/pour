#ifndef COMMON_CONSOLE_H
#define COMMON_CONSOLE_H

#include <common/common.h>
#include <stdarg.h>

typedef enum ConColor {
    COLOR_DEFAULT = 0,
    COLOR_COMMAND,
    COLOR_PROGRESS,
    COLOR_PROGRESS_SIDE,
    COLOR_STATUS,
    COLOR_SEPARATOR,
    COLOR_SUCCESS,
    COLOR_WARNING,
    COLOR_ERROR
} ConColor;

void Con_Init(void);
void Con_Terminate(void);

void Con_PrintV(lua_State* L, ConColor color, const char* fmt, va_list args);
void Con_PrintF(lua_State* L, ConColor color, const char* fmt, ...);

void Con_Flush(lua_State* L);

#endif
