#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x500
#include <windows.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef _MSC_VER
#pragma intrinsic(memcpy)
#pragma intrinsic(memset)
#endif

#ifndef JOB_OBJECT_LIMIT_KILL_ON_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_CLOSE 0x2000
#endif

#define COLOR_SUCCESS (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_ERROR (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_COMMAND (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)

static const char POUR_DIR[] = "build\\_pour";
static const char POUR_EXE[] = "\\build\\mingw32\\pour.exe";

/********************************************************************************************************************/

static HANDLE hStdOut;
static HANDLE hChildJob;
static DWORD dwChildProcessId;
static BOOL con_initialized;
static BOOL ctrl_c;
static BOOL is_console;
static BOOL print_before_pour;
static WORD default_color;
static CRITICAL_SECTION critical_section;

/********************************************************************************************************************/

static TCHAR* mycpy(TCHAR* dst, const char* src)
{
    while (*src)
        *dst++ = (TCHAR)*src++;
    return dst;
}

static TCHAR* mywcpy(TCHAR* dst, const TCHAR* src)
{
    while (*src)
        *dst++ = (TCHAR)*src++;
    return dst;
}

static TCHAR* prev_slash(TCHAR* pSlash, const TCHAR* path)
{
    while (pSlash > path) {
        --pSlash;
        if (*pSlash == TEXT('/') || *pSlash == TEXT('\\')) {
            ++pSlash;
            break;
        }
    }
    return pSlash;
}

/********************************************************************************************************************/

static void con_begin(WORD color)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    EnterCriticalSection(&critical_section);

    if (!con_initialized) {
        hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        is_console = GetConsoleScreenBufferInfo(hStdOut, &csbi);
        default_color = csbi.wAttributes;
        con_initialized = TRUE;
    }

    SetConsoleTextAttribute(hStdOut, color);
}

static void con_end(void)
{
    SetConsoleTextAttribute(hStdOut, default_color);
    LeaveCriticalSection(&critical_section);
}

static void con_printfA(WORD color, const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    int len;

    va_start(args, fmt);
    len = wvsprintfA(buf, fmt, args);
    va_end(args);

    con_begin(color);
    if (is_console)
        WriteConsoleA(hStdOut, buf, (DWORD)len, NULL, NULL);
    else {
        DWORD dwBytesWritten;
        WriteFile(hStdOut, buf, (DWORD)len, &dwBytesWritten, NULL);
    }
    con_end();
}

static void con_printW(WORD color, const TCHAR* str, ptrdiff_t len)
{
    con_begin(color);
    if (is_console)
        WriteConsole(hStdOut, str, (DWORD)len, NULL, NULL);
    else {
        static char buf[65536];
        DWORD dwBytesWritten;
        int alen = WideCharToMultiByte(CP_ACP, 0, str, (int)len, buf, (int)sizeof(buf), 0, 0);
        WriteFile(hStdOut, buf, (DWORD)alen, &dwBytesWritten, NULL);
    }
    con_end();
}

/********************************************************************************************************************/

__declspec(noreturn) static void failed(const char* func)
{
    con_printfA(COLOR_ERROR, "ERROR: %s failed: 0x%08X.\n", func, GetLastError());
    ExitProcess(1);
}

static void exec(const TCHAR* app, TCHAR* cmdline)
{
    TCHAR cwd[MAX_PATH], cmd[MAX_PATH];
    DWORD dwExitCode = (DWORD)-1;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));

    if (!app) {
        WCHAR* dst;
        dst = mycpy(cmd, "# ");
        dst = mywcpy(dst, cmdline);
        *dst++ = TEXT('\n');
        con_printW(COLOR_COMMAND, cmd, dst - cmd);
        dst = mywcpy(cmd, TEXT("cmd /C "));
        *mywcpy(dst, cmdline) = 0;
        cmdline = cmd;
    } else if (print_before_pour) {
        con_printfA(COLOR_COMMAND, ">> pour\n");
    }

    cwd[0] = 0;
    GetCurrentDirectory(MAX_PATH, cwd);

    si.cb = sizeof(si);
    if (!CreateProcess(app, cmdline, NULL, NULL, TRUE, 0, NULL, cwd, &si, &pi))
        failed("CreateProcess");

    AssignProcessToJobObject(hChildJob, pi.hProcess);

    EnterCriticalSection(&critical_section);
    dwChildProcessId = pi.dwProcessId;
    LeaveCriticalSection(&critical_section);

    WaitForSingleObject(pi.hProcess, INFINITE);

    EnterCriticalSection(&critical_section);
    dwChildProcessId = 0;
    LeaveCriticalSection(&critical_section);

    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    /*CloseHandle(pi.hThread); -- keep file size under 4K */
    /*CloseHandle(pi.hProcess); -- keep file size under 4K */

    if (dwExitCode != 0) {
        if (!app && !ctrl_c)
            con_printfA(COLOR_ERROR, "ERROR: command exited with code %u.\n", dwExitCode);
        ExitProcess(1);
    }
}

static BOOL WINAPI ctrl_handler(DWORD ctrl)
{
    switch (ctrl) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            EnterCriticalSection(&critical_section);
            if (dwChildProcessId) {
                GenerateConsoleCtrlEvent(ctrl, dwChildProcessId);
                if (WaitForSingleObject(hChildJob, 50) != WAIT_OBJECT_0) {
                    GenerateConsoleCtrlEvent(ctrl, dwChildProcessId);
                    if (WaitForSingleObject(hChildJob, 50) != WAIT_OBJECT_0) {
                        TerminateJobObject(hChildJob, (DWORD)-1);
                        WaitForSingleObject(hChildJob, INFINITE);
                    }
                }
            }
            con_printfA(COLOR_ERROR, "\n\n=== Ctrl+C ===\n\n");
            ctrl_c = TRUE;
            LeaveCriticalSection(&critical_section);
            return FALSE;
        default:
            return FALSE;
    }
}

void entry(void)
{
    JOBOBJECT_BASIC_LIMIT_INFORMATION jbli;
    TCHAR path[MAX_PATH], buf[1600];
    LPTSTR pSlash;

    InitializeCriticalSection(&critical_section);

    hChildJob = CreateJobObject(NULL, NULL);
    if (!hChildJob)
        failed("CreateJobObject");

    ZeroMemory(&jbli, sizeof(jbli));
    jbli.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_CLOSE;
    SetInformationJobObject(hChildJob, JobObjectBasicLimitInformation, &jbli, sizeof(jbli));

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    *mycpy(buf, "POUR_EXECUTABLE") = 0;
    if (!GetEnvironmentVariable(buf, path, MAX_PATH)) {
        DWORD pathLen = GetModuleFileName(NULL, path, MAX_PATH);
        pSlash = path + pathLen;
        pSlash = prev_slash(pSlash, path);
        pSlash = prev_slash(pSlash - 1, path);
        pSlash = mycpy(pSlash, POUR_DIR);

        *mycpy(pSlash, "\\CMakeLists.txt") = 0;
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
            TCHAR* dst = mycpy(buf, "git clone https://github.com/thirdpartystuff/pour \"");
            memcpy(dst, path, sizeof(TCHAR) * (size_t)(pSlash - path));
            dst[pSlash - path] = 0;
            exec(NULL, buf);
            print_before_pour = TRUE;
        }

        *mycpy(pSlash, POUR_EXE) = 0;
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
            memcpy(buf, path, sizeof(TCHAR) * (size_t)(pSlash - path));
            *mycpy(&buf[pSlash - path], "\\build_mingw.cmd") = 0;
            exec(NULL, buf);
            print_before_pour = TRUE;
        }
    }

    exec(path, GetCommandLine());

    SetConsoleCtrlHandler(ctrl_handler, FALSE);

    /*CloseHandle(hChildJob); -- keep file size under 4K */
    /*DeleteCriticalSection(&critical_section); -- keep file size under 4K */

    ExitProcess(0);
}
