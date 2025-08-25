project(lua)

set(src
    lapi.c
    lapi.h
    lauxlib.c
    lauxlib.h
    lbaselib.c
    lcode.c
    lcode.h
    lcorolib.c
    lctype.c
    lctype.h
    ldblib.c
    ldebug.c
    ldebug.h
    ldo.c
    ldo.h
    ldump.c
    lfunc.c
    lfunc.h
    lgc.c
    lgc.h
    linit.c
    liolib.c
    ljumptab.h
    llex.c
    llex.h
    llimits.h
    lmathlib.c
    lmem.c
    lmem.h
    loadlib.c
    lobject.c
    lobject.h
    lopcodes.c
    lopcodes.h
    lopnames.h
    loslib.c
    lparser.c
    lparser.h
    lprefix.h
    lstate.c
    lstate.h
    lstring.c
    lstring.h
    lstrlib.c
    ltable.c
    ltable.h
    ltablib.c
    ltm.c
    ltm.h
    lua.h
    luaconf.h
    lualib.h
    lundump.c
    lundump.h
    lutf8lib.c
    lvm.c
    lvm.h
    lzio.c
    lzio.h
    )

source_group("Source Files" FILES ${src} luac.c)

add_library(lua STATIC ${src})
target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(lua PROPERTIES FOLDER "Libs")

add_executable(luac luac.c)
target_link_libraries(luac PRIVATE lua)
set_target_properties(luac PROPERTIES FOLDER "Tools")
