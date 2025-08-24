#ifndef COMMON_CONSOLE_H
#define COMMON_CONSOLE_H

#include <common/common.h>
#include <stdarg.h>

typedef enum ConColor {
    COLOR_DEFAULT = 0,
    COLOR_COMMAND,
    COLOR_ERROR
} ConColor;

void Con_Init(void);
void Con_Terminate(void);

void Con_PrintV(ConColor color, const char* fmt, va_list args);
void Con_PrintF(ConColor color, const char* fmt, ...);

#endif
