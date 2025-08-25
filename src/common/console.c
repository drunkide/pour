#include <common/console.h>
#include <common/utf8.h>
#include <common/script.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE g_hStdOut;
static WORD g_defaultColor;
static CRITICAL_SECTION g_criticalSection;
#endif

static bool g_initialized;
static bool g_isatty;

void Con_Init()
{
    if (g_initialized)
        return;

  #ifdef _WIN32
    g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    g_isatty = GetConsoleScreenBufferInfo(g_hStdOut, &csbi);
    g_defaultColor = csbi.wAttributes;

    InitializeCriticalSection(&g_criticalSection);
  #endif

    g_initialized = true;
    atexit(Con_Terminate);
}

void Con_Terminate()
{
    if (!g_initialized)
        return;

  #ifdef _WIN32
    DeleteCriticalSection(&g_criticalSection);
  #endif

    g_initialized = false;
}

void Con_PrintV(ConColor color, const char* fmt, va_list args)
{
    const char* str = lua_pushvfstring(gL, fmt, args);

  #ifdef _WIN32

    WORD wAttr;
    switch (color) {
        case COLOR_DEFAULT: default: wAttr = g_defaultColor; break;
        case COLOR_COMMAND: wAttr = (FOREGROUND_GREEN | FOREGROUND_BLUE); break;
        case COLOR_WARNING: wAttr = (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); break;
        case COLOR_ERROR: wAttr = (FOREGROUND_RED | FOREGROUND_INTENSITY); break;
    }

    if (!g_isatty) {
        DWORD dwBytesWritten;
        WriteFile(g_hStdOut, str, strlen(str), &dwBytesWritten, NULL);
    } else {
        size_t len = 0;
        const WCHAR* str16 = (const WCHAR*)Utf8_PushConvertToUtf16(gL, str, &len);

        EnterCriticalSection(&g_criticalSection);

        SetConsoleTextAttribute(g_hStdOut, wAttr);
        WriteConsoleW(g_hStdOut, str16, (DWORD)len, NULL, NULL);
        SetConsoleTextAttribute(g_hStdOut, g_defaultColor);

        LeaveCriticalSection(&g_criticalSection);
    }

  #else

    fwrite(str, 1, strlen(str), stdout);

  #endif
}

void Con_PrintF(ConColor color, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    Con_PrintV(color, fmt, args);
    va_end(args);
}
