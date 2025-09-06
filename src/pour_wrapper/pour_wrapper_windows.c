#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x400
#include <windows.h>
#include <stddef.h>
#include <stdarg.h>

#ifndef JOB_OBJECT_LIMIT_KILL_ON_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_CLOSE 0x2000
#endif

#define COLOR_SUCCESS (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define COLOR_ERROR (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define COLOR_COMMAND (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)

static const char POUR_DIR[] = "_pour";
static const char POUR_EXE[] = "\\build\\mingw32\\pour.exe";

/********************************************************************************************************************/

typedef HANDLE (*PFNCREATEJOBOBJECTW)(LPSECURITY_ATTRIBUTES, LPWSTR);
typedef BOOL (*PFNASSIGNPROCESSTOJOBOBJECT)(HANDLE, HANDLE);
typedef BOOL (*PFNSETINFORMATIONJOBOBJECT)(HANDLE, JOBOBJECTINFOCLASS, LPVOID, DWORD);
typedef BOOL (*PFNTERMINATEJOBOBJECT)(HANDLE, UINT);

static PFNCREATEJOBOBJECTW pfnCreateJobObjectW;
static PFNASSIGNPROCESSTOJOBOBJECT pfnAssignProcessToJobObject;
static PFNSETINFORMATIONJOBOBJECT pfnSetInformationJobObject;
static PFNTERMINATEJOBOBJECT pfnTerminateJobObject;

static HANDLE hKernel32;
static HANDLE hStdOut;
static HANDLE hChildJob;
static DWORD dwChildProcessId;
static BOOL con_initialized;
static BOOL ctrl_c;
static BOOL is_console;
static BOOL print_before_pour;
static WORD default_color;
static CRITICAL_SECTION critical_section;
static JOBOBJECT_BASIC_LIMIT_INFORMATION jbli;
static STARTUPINFO si;
static PROCESS_INFORMATION pi;

/********************************************************************************************************************/

#ifndef _MSC_VER
#define copybytes memcpy
#else
static void copybytes(void* dst, const void* src, size_t size)
{
    char* d = (char*)dst;
    const char* s = (char*)src;
    while (size--)
        *d++ = *s++;
}
#endif

static TCHAR* mycpy(TCHAR* dst, const char* src)
{
    while (*src)
        *dst++ = (TCHAR)*src++;
    return dst;
}

static TCHAR* mywcpy(TCHAR* dst, const TCHAR* src)
{
    while (*src)
        *dst++ = *src++;
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
    con_printfA(COLOR_ERROR, "%s%s: %08X.\n", "ERROR: ", func, GetLastError());
    ExitProcess(1);
}

static void setChildProcessId(DWORD id)
{
    con_begin(default_color);   /* instead of EnterCriticalSection(&critical_section); */
    dwChildProcessId = id;
    con_end();                  /* instead of LeaveCriticalSection(&critical_section); */
}

static void exec(const TCHAR* app, TCHAR* cmdline)
{
    TCHAR cwd[MAX_PATH], cmd[MAX_PATH];
    DWORD dwExitCode = (DWORD)-1;

    if (!app) {
        WCHAR* dst;
        dst = mycpy(cmd, "# ");
        dst = mywcpy(dst, cmdline);
        *dst++ = TEXT('\n');
        con_printW(COLOR_COMMAND, cmd, dst - cmd);
        dst = mycpy(cmd, "cmd /C ");
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

    if (hChildJob) {
        pfnAssignProcessToJobObject(hChildJob, pi.hProcess);
        setChildProcessId(pi.dwProcessId);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    setChildProcessId(0);

    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (dwExitCode != 0) {
        if (!app && !ctrl_c)
            con_printfA(COLOR_ERROR, "%sexit code %u.\n", "ERROR: ", dwExitCode);
        ExitProcess(1);
    }

    print_before_pour = TRUE;
}

static BOOL tryKill(DWORD ctrl)
{
    GenerateConsoleCtrlEvent(ctrl, dwChildProcessId);
    return (WaitForSingleObject(hChildJob, 50) == WAIT_OBJECT_0);
}

static BOOL WINAPI ctrl_handler(DWORD ctrl)
{
    switch (ctrl) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            con_begin(COLOR_ERROR); /* instead of EnterCriticalSection(&critical_section); */
            if (dwChildProcessId) {
                if (!tryKill(ctrl) && !tryKill(ctrl)) {
                    pfnTerminateJobObject(hChildJob, (DWORD)-1);
                    WaitForSingleObject(hChildJob, INFINITE);
                }
            }
            //con_printfA(COLOR_ERROR, "\n\n=== Ctrl+C ===\n\n");
            ctrl_c = TRUE;
            con_end(); /* instead of LeaveCriticalSection(&critical_section); */
            return FALSE;
        default:
            return FALSE;
    }
}

void entry(void)
{
    static TCHAR path[MAX_PATH], buf[1600];
    LPTSTR pSlash;

    InitializeCriticalSection(&critical_section);

    hKernel32 = GetModuleHandleA("KERNEL32");
    pfnCreateJobObjectW = (PFNCREATEJOBOBJECTW)GetProcAddress(hKernel32, "CreateJobObjectW");
    pfnSetInformationJobObject = (PFNSETINFORMATIONJOBOBJECT)GetProcAddress(hKernel32, "SetInformationJobObject");
    pfnAssignProcessToJobObject = (PFNASSIGNPROCESSTOJOBOBJECT)GetProcAddress(hKernel32, "AssignProcessToJobObject");
    pfnTerminateJobObject = (PFNTERMINATEJOBOBJECT)GetProcAddress(hKernel32, "TerminateJobObject");

    if (pfnCreateJobObjectW && pfnSetInformationJobObject && pfnAssignProcessToJobObject && pfnTerminateJobObject) {
        hChildJob = pfnCreateJobObjectW(NULL, NULL);
        if (!hChildJob)
            failed("CreateJobObjectW");

        jbli.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_CLOSE;
        pfnSetInformationJobObject(hChildJob, JobObjectBasicLimitInformation, &jbli, sizeof(jbli));
    }

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    *mycpy(buf, "POUR_EXECUTABLE") = 0;
    if (!GetEnvironmentVariable(buf, path, MAX_PATH)) {
        DWORD pathLen = GetModuleFileName(NULL, path, MAX_PATH);
        pSlash = path + pathLen;
        pSlash = prev_slash(pSlash, path);
        pSlash = mycpy(pSlash, POUR_DIR);

        *mycpy(pSlash, "\\CMakeLists.txt") = 0;
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
            TCHAR* dst = mycpy(buf, "git clone https://github.com/drunkide/pour \"");
            size_t size = (size_t)(pSlash - path);
            copybytes(dst, path, sizeof(TCHAR) * size);
            dst += size;
            *dst++ = '"';
            *dst = 0;
            exec(NULL, buf);
        }

        *mycpy(pSlash, POUR_EXE) = 0;
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
            size_t size = (size_t)(pSlash - path);
            copybytes(buf, path, sizeof(TCHAR) * size);
            *mycpy(buf + size, "\\build_mingw.cmd") = 0;
            exec(NULL, buf);
        }
    }

    exec(path, GetCommandLine());

    CloseHandle(hChildJob);
    DeleteCriticalSection(&critical_section);

    ExitProcess(0);
}
