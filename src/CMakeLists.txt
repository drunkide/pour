
if(NOT MSVC)
    add_definitions(
        -Wall
        -Wextra
        -Werror
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
        )
endif()

add_subdirectory(pour_wrapper)

######################################################################################################################

set(functions_lua "${CMAKE_CURRENT_SOURCE_DIR}/../data/functions.lua")

set(src_data
    "${functions_lua}"
    )

set(src_data_packages
    ../data/packages/clang-4.0.0.lua
    ../data/packages/cmake-3.5.2.lua
    ../data/packages/cmake-3.31.4.lua
    ../data/packages/egcs-1.1.2.lua
    ../data/packages/foreign-linux.lua
    ../data/packages/mingw32-4.4.0.lua
    ../data/packages/mingw32-8.1.0.lua
    ../data/packages/mingw64-8.1.0.lua
    ../data/packages/ninja.lua
    )

set(src
    common/byteswap.h
    common/common.h
    common/console.c
    common/console.h
    common/dirs.c
    common/dirs.h
    common/env.c
    common/env.h
    common/exec.c
    common/exec.h
    common/file.c
    common/file.h
    common/script.c
    common/script.h
    common/utf8.c
    common/utf8.h
    dosbox/dosbox.c
    dosbox/dosbox.h
    dosbox/dosbox.opt
    grp/grpfile.c
    grp/grpfile.h
    mkdisk/bootcode.c
    mkdisk/bootcode.h
    mkdisk/disk_config.c
    mkdisk/disk_config.h
    mkdisk/ext2.c
    mkdisk/ext2.h
    mkdisk/ext2_defs.h
    mkdisk/fat.c
    mkdisk/fat.h
    mkdisk/fat_defs.h
    mkdisk/mbr.c
    mkdisk/mbr.h
    mkdisk/mbr_defs.h
    mkdisk/mkdisk.c
    mkdisk/mkdisk.h
    mkdisk/vhd.c
    mkdisk/vhd.h
    mkdisk/vhd_defs.h
    mkdisk/write.c
    mkdisk/write.h
    patch/patch.c
    patch/patch.h
    pour/pour.c
    pour/pour.h
    pour/pour_lua.c
    pour/pour_lua.h
    _main.c
    )

source_group("Data Files" FILES ${src_data})
source_group("Data Files\\packages" FILES ${src_data_packages})

set(src_all)
foreach(file ${src})
    get_filename_component(abs "${file}" ABSOLUTE)
    list(APPEND src_all "${abs}")
    get_filename_component(path "${file}" DIRECTORY)
    string(REPLACE "/" "\\" group "Source Files/${path}")
    source_group("${group}" FILES "${abs}")
endforeach()

set(functions_lua_h "${CMAKE_CURRENT_BINARY_DIR}/functions.lua.h")
add_custom_command(OUTPUT
        "${functions_lua_h}"
    COMMAND
        luac -c -o "${functions_lua_h}" "${functions_lua}"
    DEPENDS
        "${functions_lua}"
        luac
    WORKING_DIRECTORY
        "${lua_BINARY_DIR}"
    )

source_group("Generated Files" FILES "${functions_lua_h}")
list(APPEND src_all "${functions_lua_h}")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
add_definitions(-DPOUR_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(pour ${src_all})
target_link_libraries(pour PRIVATE lua)

if(NOT MSVC)
    set_target_properties(pour PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
