#include <common/script.h>
#include <common/dirs.h>
#include <common/console.h>
#include <common/utf8.h>
#include <common/exec.h>
#include <common/file.h>
#include <dosbox/dosbox.h>
#include <mkdisk/mkdisk.h>
#include <patch/patch.h>
#include <pour/pour_lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include "functions.lua.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

lua_State* gL;

/********************************************************************************************************************/

/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State* L, lua_Debug* ar)
{
    (void)ar;  /* unused arg. */
    lua_sethook(L, NULL, 0, 0);  /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i)
{
    int flag = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(gL, lstop, flag, 1);
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
        Con_PrintF(L, COLOR_ERROR, "ERROR: %s\n", msg);
        lua_pop(L, 1);
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
    signal(SIGINT, laction);  /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

bool Script_DoFile(lua_State* L, const char* name, const char* chdir, int globalsTableIdx)
{
    int n = lua_gettop(L);

    File_PushCurrentDirectory(L);
    int curdir = lua_gettop(L);

    char path[DIR_MAX];
    strcpy(path, name);
    Dir_MakeAbsolutePath(L, path, sizeof(path));
    Dir_FromNativeSeparators(path);
    Dir_RemoveLastPath(path);

    if (globalsTableIdx != 0)           /* new _ENV */
        lua_pushvalue(L, globalsTableIdx);
    else
        lua_newtable(L);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "_G");          /* _ENV._G = _ENV */
    lua_pushstring(L, path);
    lua_setfield(L, -2, "SCRIPT_DIR");  /* _ENV.SCRIPT_DIR = <path> */

    lua_newtable(L);                    /* metatable for new _ENV */
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);

    int status = report(L, luaL_loadfile(L, name)); /* FIXME: utf-8 */
    if (status != LUA_OK) {
        lua_settop(L, n);
        return false;
    }

    if (chdir)
        File_SetCurrentDirectory(L, chdir);

    lua_pushvalue(L, -2);               /* new _ENV */
    lua_setupvalue(L, -2, 1);           /* set as upvalue #1 for the script */

    status = report(L, docall(L, 0, 0));

    const char* oldcwd = lua_tostring(L, curdir);
    File_SetCurrentDirectory(L, oldcwd);

    lua_settop(L, n);
    return status == LUA_OK;
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
    lua_setglobal(L, "WINDOWS");

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

    WCHAR* buf = (WCHAR*)alloca(sizeof(WCHAR) * MAX_PATH);
    if (GetModuleFileNameW(NULL, buf, MAX_PATH))
        params->argv[0] = (char*)Utf8_PushConvertFromUtf16(L, buf);
  #endif

    Dirs_Init(L);

    lua_pushstring(L, g_rootDir); lua_setglobal(L, "ROOT_DIR");
    lua_pushstring(L, g_installDir); lua_setglobal(L, "INSTALL_DIR");
    lua_pushstring(L, g_dataDir); lua_setglobal(L, "DATA_DIR");
    lua_pushstring(L, g_packagesDir); lua_setglobal(L, "PACKAGES_DIR");

    Exec_Init(L);

    luaL_openlibs(L);
    Pour_InitLua(L);
    MkDisk_InitLua(L);
    Patch_InitLua(L);
    DOSBox_InitLua(L);

    int status = luaL_loadbuffer(L, (const char*)functions_lua, sizeof(functions_lua), "@functions.lua");
    if (report(L, status) != 0)
        return 0;
    lua_call(L, 0, 0);

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

    lua_close(L);

    return (result && status == LUA_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}
