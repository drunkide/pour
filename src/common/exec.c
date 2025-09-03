#include <common/exec.h>
#include <common/console.h>
#include <common/dirs.h>
#include <common/script.h>
#include <common/utf8.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef JOB_OBJECT_LIMIT_KILL_ON_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_CLOSE 0x2000
#endif
static bool g_ctrlC;
static CRITICAL_SECTION g_criticalSection;
static HANDLE g_hChildJob;
static DWORD g_dwChildProcessId;
#endif

static bool g_initialized;
bool g_dont_print_commands;

#ifdef _WIN32
static BOOL WINAPI Exec_CtrlHandler(DWORD ctrl)
{
    switch (ctrl) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            EnterCriticalSection(&g_criticalSection);
            if (g_dwChildProcessId) {
                GenerateConsoleCtrlEvent(ctrl, g_dwChildProcessId);
                if (WaitForSingleObject(g_hChildJob, 50) != WAIT_OBJECT_0) {
                    GenerateConsoleCtrlEvent(ctrl, g_dwChildProcessId);
                    if (WaitForSingleObject(g_hChildJob, 50) != WAIT_OBJECT_0) {
                        TerminateJobObject(g_hChildJob, (DWORD)-1);
                        WaitForSingleObject(g_hChildJob, INFINITE);
                    }
                }
            }
            g_ctrlC = TRUE;
            Script_Interrupt();
            LeaveCriticalSection(&g_criticalSection);
            return FALSE;
        default:
            return FALSE;
    }
}
#endif

void Exec_Init(lua_State* L)
{
    if (g_initialized)
        return;

  #ifdef _WIN32

    InitializeCriticalSection(&g_criticalSection);

    g_hChildJob = CreateJobObject(NULL, NULL);
    if (!g_hChildJob)
        luaL_error(L, "CreateJobObject failed (code 0x%p).", (void*)(size_t)GetLastError());

    JOBOBJECT_BASIC_LIMIT_INFORMATION jbli;
    ZeroMemory(&jbli, sizeof(jbli));
    jbli.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_CLOSE;
    SetInformationJobObject(g_hChildJob, JobObjectBasicLimitInformation, &jbli, sizeof(jbli));

    SetConsoleCtrlHandler(Exec_CtrlHandler, TRUE);

  #endif

    g_initialized = true;
    atexit(Exec_Terminate);
}

void Exec_Terminate()
{
    if (!g_initialized)
        return;

  #ifdef _WIN32
    SetConsoleCtrlHandler(Exec_CtrlHandler, FALSE);
    DeleteCriticalSection(&g_criticalSection);
  #endif

    g_initialized = false;
}

static void pushArgument(lua_State* L, const char* argument)
{
    bool needEscape = false;

    for (const char* p = argument; *p; ++p) {
        if (*p == ' ') {
            needEscape = true;
            break;
        }
    }

    if (needEscape)
        lua_pushliteral(L, "\"");
    lua_pushstring(L, argument);
    if (needEscape)
        lua_pushliteral(L, "\"");
}

bool Exec_Command(lua_State* L, const char* const* argv, int argc, const char* chdir)
{
    return Exec_CommandV(L, argv[0], argv, argc, chdir, RUN_WAIT);
}

bool Exec_CommandV(lua_State* L, const char* command, const char* const* argv, int argc,
    const char* chdir, runmode_t mode)
{
    int start = lua_gettop(L);
    int argStart = start;

    luaL_checkstack(L, 100, NULL);

  #ifdef _WIN32
    size_t commandLen = strlen(command);
    char* commandBuf = (char*)lua_newuserdatauv(L, commandLen + 1, 0);
    memcpy(commandBuf, command, commandLen);
    commandBuf[commandLen] = 0;
    Dir_ToNativeSeparators(commandBuf);
    command = commandBuf;
    ++argStart;
    lua_pushliteral(L, "cmd /C ");
  #endif
    pushArgument(L, command);
    for (int i = 1; i < argc; i++) {
        lua_pushliteral(L, " ");
        pushArgument(L, argv[i]);
    }
    lua_concat(L, lua_gettop(L) - argStart);
    const char* cmd = lua_tostring(L, -1);

    if (!g_dont_print_commands)
        Con_PrintF(L, COLOR_COMMAND, "# %s\n", cmd);

  #ifdef _WIN32

    WCHAR* cmd16 = (WCHAR*)Utf8_PushConvertToUtf16(L, cmd, NULL);
    WCHAR* cwd, cwdbuf[MAX_PATH];

    if (chdir)
        cwd = (WCHAR*)Utf8_PushConvertToUtf16(L, chdir, NULL);
    else {
        cwdbuf[0] = 0;
        GetCurrentDirectoryW(MAX_PATH, cwdbuf);
        cwd = cwdbuf;
    }

    BOOL bInheritHandles = TRUE;
    DWORD dwCreationFlags = CREATE_DEFAULT_ERROR_MODE;

    switch (mode) {
        case RUN_WAIT:
            break;
        case RUN_DONT_WAIT:
            bInheritHandles = FALSE;
            dwCreationFlags |= CREATE_NEW_CONSOLE;
            break;
        case RUN_DONT_WAIT_NO_CONSOLE:
            bInheritHandles = FALSE;
            dwCreationFlags |= DETACHED_PROCESS;
            break;
    }

    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessW(NULL, cmd16, NULL, NULL, bInheritHandles, dwCreationFlags, NULL, cwd, &si, &pi)) {
        Con_PrintF(L, COLOR_ERROR, "ERROR: CreateProcess failed (code 0x%p).\n", (void*)(size_t)GetLastError());
        lua_settop(L, start);
        return false;
    }

    if (mode != RUN_WAIT) {
        lua_settop(L, start);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    AssignProcessToJobObject(g_hChildJob, pi.hProcess);

    EnterCriticalSection(&g_criticalSection);
    g_dwChildProcessId = pi.dwProcessId;
    LeaveCriticalSection(&g_criticalSection);

    WaitForSingleObject(pi.hProcess, INFINITE);

    EnterCriticalSection(&g_criticalSection);
    g_dwChildProcessId = 0;
    LeaveCriticalSection(&g_criticalSection);

    DWORD dwExitCode = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (dwExitCode != 0) {
        if (!g_ctrlC)
            Con_PrintF(L, COLOR_ERROR, "ERROR: command exited with code %d.\n", (int)dwExitCode);
        lua_settop(L, start);
        return false;
    }

  #else

    if (system(cmd) != 0) {
        lua_settop(L, start);
        return false;
    }

  #endif

    lua_settop(L, start);
    return true;
}
