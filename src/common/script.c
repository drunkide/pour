#include <common/script.h>
#include <common/dirs.h>
#include <common/console.h>
#include <common/utf8.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

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
        Con_PrintF(COLOR_ERROR, "error: %s\n", msg);
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
        params->argv = (char**)lua_newuserdata(L, sizeof(char*) * wargc);
        for (int i = 0; i < wargc; i++) {
            params->argv[i] = (char*)Utf8_PushConvertFromUtf16(L, wargv[i]);
            luaL_ref(L, LUA_REGISTRYINDEX);
        }
        LocalFree(wargv);
    }
  #endif

    Dirs_Init();

    lua_pushstring(L, g_rootDir); lua_setglobal(L, "ROOT_DIR");
    lua_pushstring(L, g_toolsDir); lua_setglobal(L, "TOOLS_DIR");
    lua_pushstring(L, g_dataDir); lua_setglobal(L, "DATA_DIR");
    lua_pushstring(L, g_packagesDir); lua_setglobal(L, "PACKAGES_DIR");

    luaL_openlibs(L);

    lua_gc(L, LUA_GCRESTART);  /* start GC... */
    lua_gc(L, LUA_GCGEN, 0, 0);  /* ...in generational mode */

    if (!params->pfnMain(params->argc, params->argv))
        return 0;

    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

/********************************************************************************************************************/

bool Script_DoFile(const char* name)
{
    lua_State* L = gL;

    int status = luaL_loadfile(L, name);
    if (status == LUA_OK)
        status = docall(L, 0, 0);

    return report(L, status) == LUA_OK;
}

int Script_RunVM(int argc, char** argv, PFNMainProc pfnMain)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "error: Lua initialization failed.\n");
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
