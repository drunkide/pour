cmake_minimum_required(VERSION 3.5...3.31)
project(pour)

set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

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

add_subdirectory(lib)
add_subdirectory(src)
