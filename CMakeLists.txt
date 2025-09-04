cmake_minimum_required(VERSION 3.5...3.31)
project(pour)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/data/cmake")
include(DrunkFly/Common)

if(USE_POSIX_IO)
    add_definitions(-DUSE_POSIX_IO)
endif()

set_directory_properties(PROPERTIES
    VS_STARTUP_PROJECT pour
    )

set(CMAKE_C_STANDARD 99)
if(NOT MSVC)
    add_definitions(-fno-ident)
endif()

add_definitions(-DLUA_USE_APICHECK)

if(MSVC)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/dirent")
endif()
if(CLANG AND WIN32)
    add_definitions(-D__USE_MINGW_ANSI_STDIO)
endif()

add_subdirectory(lib)
add_subdirectory(src)
