#include <common/console.h>
#include <common/file.h>
#include <pour/pour.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

static const char* g_configSys = NULL;
static const char* g_autoexecBat = NULL;

static const char* pinned_string(lua_State* L, int index)
{
    const char* str = luaL_checkstring(L, index);
    lua_pushvalue(L, index);
    luaL_ref(L, LUA_REGISTRYINDEX);
    return str;
}

static const char* pinned_concat_string(lua_State* L, const char* old, int index,
    const char* prepend, const char* append)
{
    const char* str = luaL_checkstring(L, index);
    int n = lua_gettop(L);

    if (old)
        lua_rawgetp(L, LUA_REGISTRYINDEX, old);
    if (prepend)
        lua_pushstring(L, prepend);
    lua_pushvalue(L, index);
    if (append)
        lua_pushstring(L, append);

    lua_concat(L, lua_gettop(L) - n);
    str = lua_tostring(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, str); /* keep string in memory until Lua exits */

    return str;
}

#define DOSBOX_OPTION(NAME, DEFAULT) \
    static const char* opt_##NAME = DEFAULT; \
    static int dosbox_set_##NAME(lua_State* L) { \
        opt_##NAME = pinned_string(L, 1); \
        return 0; \
    }
#include "dosbox.opt"

static int dosbox_add_autoexec_bat(lua_State* L)
{
    g_autoexecBat = pinned_concat_string(L, g_autoexecBat, 1, "@", "\n");
    return 0;
}

static int dosbox_add_config_sys(lua_State* L)
{
    g_configSys = pinned_concat_string(L, g_configSys, 1, NULL, "\n");
    return 0;
}

static void dosbox_write_config(lua_State* L, const char* path)
{
    luaL_checkstack(L, 1000, NULL);

    #define OPT_pushfstring(L, fmt, var) \
        if (var) lua_pushfstring(L, fmt, var)

    int n = lua_gettop(L);
    lua_pushliteral(L, "[sdl]\n");
    lua_pushliteral(L, "fullscreen=false\n");
    lua_pushliteral(L, "fulldouble=false\n");
    lua_pushliteral(L, "fullresolution=original\n");
    lua_pushliteral(L, "windowposition=,\n");
    lua_pushfstring(L, "windowresolution=%s\n", opt_window_resolution);
    lua_pushliteral(L, "output=opengl\n");
    lua_pushliteral(L, "autolock=true\n");
    lua_pushliteral(L, "autolock_feedback=flash\n");
    lua_pushliteral(L, "sensitivity=100\n");
    lua_pushliteral(L, "waitonerror=true\n");
    lua_pushliteral(L, "priority=higher,normal\n");
    lua_pushliteral(L, "usescancodes=true\n");
    OPT_pushfstring(L, "mouse_emulation=%s\n", opt_mouse_emulation);
    OPT_pushfstring(L, "mouse_wheel_key=%s\n", opt_mouse_wheel_key);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[dosbox]\n");
    lua_pushfstring(L, "title=%s\n", opt_title);
    lua_pushliteral(L, "fastbioslogo=true\n");
    lua_pushliteral(L, "startbanner=false\n");
    lua_pushliteral(L, "quit warning=false\n");
    lua_pushliteral(L, "language=\n");
    OPT_pushfstring(L, "machine=%s\n", opt_machine);
    lua_pushliteral(L, "captures=capture\n");
    lua_pushfstring(L, "memsize=%s\n", opt_mem_size);
    lua_pushliteral(L, "nocachedir=true\n");
    OPT_pushfstring(L, "unmask keyboard on int 16 read=%s\n", opt_unmask_keyboard_on_int16_read);
    lua_pushliteral(L, "dpi aware=true\n");
    lua_pushliteral(L, "convertdrivefat=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[video]\n");
    OPT_pushfstring(L, "vmemsize=%s\n", opt_video_mem_size);
    lua_pushliteral(L, "vesa modelist width limit=0\n");
    lua_pushliteral(L, "vesa modelist height limit=0\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[render]\n");
    lua_pushliteral(L, "frameskip=0\n");
    lua_pushliteral(L, "aspect=false\n");
    lua_pushliteral(L, "scaler=normal3x\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[cpu]\n");
    lua_pushfstring(L, "core=%s\n", opt_cpu_core);
    lua_pushliteral(L, "cputype=pentium\n");
    lua_pushfstring(L, "cycles=%s\n", opt_cpu_cycles);
    lua_pushliteral(L, "cycleup=10000\n");
    lua_pushliteral(L, "cycledown=10000\n");
    OPT_pushfstring(L, "integration device=%s\n", opt_integration_device);
    OPT_pushfstring(L, "isapnpbios=%s\n", opt_isa_pnp_bios);
    OPT_pushfstring(L, "isapnpport=%s\n", opt_isa_pnp_port);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[voodoo]\n");
    lua_pushliteral(L, "voodoo_card=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[mixer]\n");
    lua_pushliteral(L, "nosound=false\n");
    lua_pushliteral(L, "rate=44100\n");
    lua_pushliteral(L, "blocksize=1024\n");
    lua_pushliteral(L, "prebuffer=25\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[midi]\n");
    lua_pushliteral(L, "mpu401=intelligent\n");
    lua_pushliteral(L, "mididevice=default\n");
    lua_pushliteral(L, "midiconfig=\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[sblaster]\n");
    lua_pushfstring(L, "sbtype=%s\n", opt_sblaster_type);
    lua_pushliteral(L, "sbbase=220\n");
    lua_pushliteral(L, "irq=7\n");
    lua_pushliteral(L, "dma=1\n");
    lua_pushliteral(L, "hdma=5\n");
    lua_pushliteral(L, "sbmixer=true\n");
    lua_pushliteral(L, "oplmode=auto\n");
    lua_pushliteral(L, "oplemu=default\n");
    lua_pushliteral(L, "oplrate=44100\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[gus]\n");
    lua_pushliteral(L, "gus=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[speaker]\n");
    lua_pushliteral(L, "pcspeaker=true\n");
    lua_pushliteral(L, "pcrate=44100\n");
    lua_pushliteral(L, "tandy=auto\n");
    lua_pushliteral(L, "tandyrate=44100\n");
    lua_pushliteral(L, "disney=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[joystick]\n");
    lua_pushliteral(L, "joysticktype=auto\n");
    lua_pushliteral(L, "timed=true\n");
    lua_pushliteral(L, "autofire=false\n");
    lua_pushliteral(L, "swap34=false\n");
    lua_pushliteral(L, "buttonwrap=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[serial]\n");
    lua_pushfstring(L, "serial1=%s\n", opt_serial1);
    lua_pushliteral(L, "serial2=disabled\n");
    lua_pushliteral(L, "serial3=disabled\n");
    lua_pushliteral(L, "serial4=disabled\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[parallel]\n");
    lua_pushliteral(L, "parallel1=disabled\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[printer]\n");
    lua_pushliteral(L, "printer=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[dos]\n");
    lua_pushfstring(L, "xms=%s\n", opt_xms);
    lua_pushfstring(L, "ems=%s\n", opt_ems);
    lua_pushfstring(L, "umb=%s\n", opt_umb);
    lua_pushliteral(L, "network redirector=false\n");
    lua_pushliteral(L, "keyboardlayout=auto\n");
    lua_pushliteral(L, "hard drive data rate limit=0\n");
    lua_pushliteral(L, "floppy drive data rate limit=0\n");
    lua_pushliteral(L, "ver=7.0\n");
    lua_pushliteral(L, "lfn=true\n");
    OPT_pushfstring(L, "biosps2=%s\n", opt_bios_ps2);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[keyboard]\n");
    OPT_pushfstring(L, "aux=%s\n", opt_keyboard_aux);
    OPT_pushfstring(L, "controller=%s\n", opt_keyboard_controller);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[ide]\n");
    lua_pushliteral(L, "model=buslogic\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[fdc, primary]\n");
    lua_pushfstring(L, "enable=%s\n", opt_fdc_enable);
    OPT_pushfstring(L, "int13fakev86io=%s\n", opt_int13_fake_v86_io);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[ide, primary]\n");
    lua_pushliteral(L, "int13fakeio=true\n");
    OPT_pushfstring(L, "int13fakev86io=%s\n", opt_int13_fake_v86_io);
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[ipx]\n");
    lua_pushliteral(L, "ipx=false\n");
    lua_pushliteral(L, "\n");
    lua_pushliteral(L, "[ne2000]\n");
    lua_pushliteral(L, "ne2000=true\n");
    lua_pushliteral(L, "nicbase=300\n");
    lua_pushliteral(L, "nicirq=3\n");
    lua_pushliteral(L, "backend=slirp\n");
    lua_pushliteral(L, "macaddr=BA:D0:C0:DE:CC:DD\n");
    if (g_configSys) {
        lua_pushliteral(L, "\n");
        lua_pushliteral(L, "[config]\n");
        lua_rawgetp(L, LUA_REGISTRYINDEX, g_configSys);
    }
    if (g_autoexecBat) {
        lua_pushliteral(L, "\n");
        lua_pushliteral(L, "[autoexec]\n");
        lua_rawgetp(L, LUA_REGISTRYINDEX, g_autoexecBat);
    }
    lua_concat(L, lua_gettop(L) - n);

    size_t length;
    const char* data = lua_tolstring(L, -1, &length);
    if (File_MaybeOverwrite(L, path, data, length))
        Con_PrintF(L, COLOR_WARNING, "DOSBox: wrote new config file.\n");
    else
        Con_PrintF(L, COLOR_SUCCESS, "DOSBox: config file unchanged.\n");
}

static int dosbox_run(lua_State* L)
{
    dosbox_write_config(L, ".dosbox.conf");

    const char* argv[4];
    argv[0] = "DOSBox-x";
    argv[1] = "-noconsole";
    argv[2] = "-conf";
    argv[3] = ".dosbox.conf";

    if (!Pour_Run(L, "dosbox-x", NULL, 4, (char**)argv, RUN_DONT_WAIT_NO_CONSOLE))
        return luaL_error(L, "unable to launch DOSBox.");

    return 0;
}

static const luaL_Reg funcs[] = {
    #define DOSBOX_OPTION(NAME, DEFAULT) \
        { #NAME, dosbox_set_##NAME },
    #include "dosbox.opt"
    { "autoexec_bat", dosbox_add_autoexec_bat },
    { "config_sys", dosbox_add_config_sys },
    { "run", dosbox_run },
    { NULL, NULL }
};

static int luaopen_dosbox(lua_State* L)
{
    luaL_newlib(L, funcs);
    return 1;
}

void DOSBox_InitLua(lua_State* L)
{
    luaL_requiref(L, "dosbox", luaopen_dosbox, 1);
    lua_pop(L, 1);
}
