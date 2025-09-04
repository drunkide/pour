#include <common/script.h>
#include <common/dirs.h>
#include <common/console.h>
#include <common/utf8.h>
#include <common/exec.h>
#include <common/file.h>
#include <grp/grpfile.h>
#include <dosbox/dosbox.h>
#include <mkdisk/mkdisk.h>
#include <mkdisk/ext2read.h>
#include <patch/patch.h>
#include <pour/pour_lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "functions.lua.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

lua_State* gL;

static bool g_exited;
static bool g_cleanExit;

static const char* g_currentScriptDir;
static volatile int g_inCall;

/********************************************************************************************************************/

bool Script_IsAbnormalTermination(lua_State* L)
{
    DONT_WARN_UNUSED(L);
    return g_exited && !g_cleanExit;
}

void Script_GetString(lua_State* L, int index, char* buf, size_t bufSize, const char* error)
{
    size_t len;
    const char* src = lua_tolstring(L, index, &len);
    if (len >= bufSize)
        luaL_error(L, "%s: %s", error, src);
    memcpy(buf, src, len + 1);
}

const char* Script_GetCurrentScriptDir(lua_State* L)
{
    DONT_WARN_UNUSED(L);
    return g_currentScriptDir;
}

/********************************************************************************************************************/

/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State* L, lua_Debug* ar)
{
    DONT_WARN_UNUSED(ar);
    /*lua_sethook(L, NULL, 0, 0);*/  /* reset hook */
    luaL_error(L, "interrupted!");
}

void Script_Interrupt(void)
{
    if (!g_inCall)
        return;

    const int flag = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
    lua_sethook(gL, lstop, flag, 1);
}

/********************************************************************************************************************/

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i)
{
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    Script_Interrupt();
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
            return 1;
        msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack.
*/
static int report(lua_State* L, int status)
{
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        if (msg == NULL || !*msg)
            msg = "(error message not a string)";

        const char* p = strstr(msg, "\nstack traceback");
        if (!p) {
            Con_PrintF(L, COLOR_ERROR, "\nERROR: %s\n", msg);
            lua_pop(L, 1);
        } else {
            lua_pushlstring(L, msg, (size_t)(p - msg));
            Con_PrintF(L, COLOR_ERROR, "\nERROR: %s\n", lua_tostring(L, -1));
            Con_Print(L, COLOR_TRACEBACK, p + 1);
            lua_pop(L, 2);
        }
    }

    return status;
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State* L, int narg, int nres)
{
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */

    if (g_inCall++ == 0)
        signal(SIGINT, laction);  /* set C-signal handler */

    status = lua_pcall(L, narg, nres, base);

    if (--g_inCall == 0)
        signal(SIGINT, SIG_DFL); /* reset C-signal handler */

    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

static int pushGlobals(lua_State* L, int globalsTableIdx)
{
    int envIndex;
    if (globalsTableIdx != 0)                   /* new _ENV */
        envIndex = globalsTableIdx;
    else {
        lua_newtable(L);
        envIndex = lua_gettop(L);
    }

    lua_pushvalue(L, -1);
    lua_setfield(L, envIndex, "_G");            /* _ENV._G = _ENV */

    /* copy values from globals */
    lua_pushglobaltable(L);
    int globalsIndex = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, globalsIndex)) {
        lua_pushvalue(L, -2);
        if (lua_rawget(L, envIndex) > LUA_TNIL)
            lua_pop(L, 1); /* override exists, keep it */
        else {
            lua_pop(L, 1);
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -2);
            lua_rawset(L, envIndex);
        }
        lua_pop(L, 1);
    }

    lua_pushvalue(L, envIndex);                 /* new _ENV */

    return envIndex;
}

bool Script_DoFile(lua_State* L, const char* name, const char* chdir, int globalsTableIdx)
{
    int n = lua_gettop(L);

    char path[DIR_MAX];
    strcpy(path, name); /* FIXME: possible overflow */
    Dir_MakeAbsolutePath(L, path, sizeof(path));
    Dir_FromNativeSeparators(path);
    Dir_RemoveLastPath(path);

    int status = report(L, luaL_loadfile(L, name)); /* FIXME: utf-8 */
    if (status != LUA_OK) {
        lua_settop(L, n);
        return false;
    }

    int functionIdx = lua_gettop(L);

    int envIndex = pushGlobals(L, globalsTableIdx);
    lua_pushstring(L, path);
    lua_setfield(L, envIndex, "SCRIPT_DIR");    /* _ENV.SCRIPT_DIR = <path> */
    lua_setupvalue(L, functionIdx, 1);          /* set as upvalue #1 for the script */

    bool result = Script_DoFunction(L, path, chdir, functionIdx);

    lua_settop(L, n);
    return result;
}

bool Script_DoFunction(lua_State* L, const char* scriptDir, const char* chdir, int functionIdx)
{
    int n = lua_gettop(L);

    File_PushCurrentDirectory(L);
    int curdir = lua_gettop(L);

    if (chdir)
        File_SetCurrentDirectory(L, chdir);

    lua_pushvalue(L, functionIdx);

    const char* prevScriptDir = g_currentScriptDir;
    g_currentScriptDir = scriptDir;
    int status = docall(L, 0, 0);
    g_currentScriptDir = prevScriptDir;

    report(L, status);

    const char* oldcwd = lua_tostring(L, curdir);
    File_SetCurrentDirectory(L, oldcwd);

    lua_settop(L, n);
    return status == LUA_OK;
}

/********************************************************************************************************************/

bool Script_LoadFunctions(lua_State* L, int globalsTableIdx)
{
    int status = luaL_loadbuffer(L, (const char*)functions_lua, sizeof(functions_lua), "@functions.lua");
    if (report(L, status) != 0)
        return false;

    if (globalsTableIdx != 0) {
        int functionIdx = lua_gettop(L);
        pushGlobals(L, globalsTableIdx);
        lua_setupvalue(L, functionIdx, 1); /* set as upvalue #1 for the script */
        lua_pushvalue(L, functionIdx);
    }

    lua_call(L, 0, 0);
    return true;
}

/********************************************************************************************************************/

STRUCT(MainParams) {
    PFNMainProc pfnMain;
    char** argv;
    int argc;
};

/*
** Main body of the program (to be called in protected mode).
*/
static int pmain(lua_State *L)
{
    MainParams* params = (MainParams*)lua_touserdata(L, 1);

    lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");

    Con_Init();

  #ifdef _WIN32
    lua_pushboolean(L, 1);
    lua_setglobal(L, "HOST_WINDOWS");

    int wargc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
        params->argc = wargc;
        params->argv = (char**)lua_newuserdatauv(L, sizeof(char*) * wargc, 0);
        for (int i = 0; i < wargc; i++) {
            params->argv[i] = (char*)Utf8_PushConvertFromUtf16(L, wargv[i]);
            luaL_ref(L, LUA_REGISTRYINDEX);
        }
        LocalFree(wargv);
    }

    WCHAR* buf = (WCHAR*)lua_newuserdatauv(L, sizeof(WCHAR) * MAX_PATH, 0);
    DWORD filenameLen = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (filenameLen == 0 || filenameLen == MAX_PATH)
        luaL_error(L, "GetModuleFileName() failed (code %p).", (void*)(size_t)GetLastError());
    params->argv[0] = (char*)Utf8_PushConvertFromUtf16(L, buf);
    lua_replace(L, -2);
  #endif

    Dirs_Init(L);

    lua_pushstring(L, g_rootDir); lua_setglobal(L, "ROOT_DIR");
    lua_pushstring(L, g_installDir); lua_setglobal(L, "INSTALL_DIR");
    lua_pushstring(L, g_dataDir); lua_setglobal(L, "DATA_DIR");
    lua_pushstring(L, g_packagesDir); lua_setglobal(L, "PACKAGES_DIR");
    lua_pushstring(L, g_targetsDir); lua_setglobal(L, "TARGETS_DIR");

    Exec_Init(L);

    luaL_openlibs(L);
    Pour_InitLua(L);
    MkDisk_InitLua(L);
    Patch_InitLua(L);
    GrpFile_InitLua(L);
    DOSBox_InitLua(L);
    Ext2Read_InitLua(L);

    if (!Script_LoadFunctions(L, 0))
        return 0;

    lua_gc(L, LUA_GCRESTART);  /* start GC... */
    lua_gc(L, LUA_GCGEN, 0, 0);  /* ...in generational mode */

    if (!params->pfnMain(L, params->argc, params->argv))
        return 0;

    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

int Script_RunVM(int argc, char** argv, PFNMainProc pfnMain)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "ERROR: Lua initialization failed.\n");
        return EXIT_FAILURE;
    }

    gL = L;

    lua_gc(L, LUA_GCSTOP);

    MainParams params;
    params.pfnMain = pfnMain;
    params.argv = argv;
    params.argc = argc;

    lua_pushcfunction(L, &pmain);
    lua_pushlightuserdata(L, &params);
    int status = lua_pcall(L, 1, 1, 0);
    int result = lua_toboolean(L, -1);
    report(L, status);

    g_exited = true;
    g_cleanExit = (result && status == LUA_OK);

    lua_close(L);

    return (g_cleanExit ? EXIT_SUCCESS : EXIT_FAILURE);
}
