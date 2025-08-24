
set(src_data_packages
    ../data/packages/cmake-3.31.4.lua
    ../data/packages/cmake-3.5.2.lua
    ../data/packages/mingw32-4.4.0.lua
    )

set(src
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
    pour/pour.c
    pour/pour.h
    _main.c
    )

source_group("Data Files\\packages" FILES ${src_data_packages})

set(src_all)
foreach(file ${src})
    get_filename_component(abs "${file}" ABSOLUTE)
    list(APPEND src_all "${abs}")
    get_filename_component(path "${file}" DIRECTORY)
    string(REPLACE "/" "\\" group "Source Files/${path}")
    source_group("${group}" FILES "${abs}")
endforeach()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DPOUR_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    add_definitions(
        -Wall
        -Wextra
        -Werror
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
        )
endif()

add_executable(pour ${src_all})
target_link_libraries(pour PRIVATE lua)

if(WIN32)
    set_target_properties(pour PROPERTIES
        SUFFIX ".exe")
endif()

if(NOT MSVC)
    set_target_properties(pour PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

add_subdirectory(pour_wrapper)
