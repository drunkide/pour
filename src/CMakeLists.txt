
set(src_data_packages
    ../data/packages/cmake-3.31.4.lua
    ../data/packages/cmake-3.5.2.lua
    ../data/packages/mingw32-4.4.0.lua
    )

set(src_common
    common/common.h
    common/dirs.c
    common/dirs.h
    common/exec.c
    common/exec.h
    common/file.c
    common/file.h
    common/script.c
    common/script.h
    )

set(src_pour
    pour/pour.c
    pour/pour.h
    )

set(src
    _main.c
    )

source_group("Data Files\\packages" FILES ${src_data_packages})
source_group("Source Files\\common" FILES ${src_common})
source_group("Source Files\\pour" FILES ${src_pour})
source_group("Source Files" FILES ${src})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    add_definitions(-Wall -Wextra -Werror)
endif()

add_executable(pour
    ${src_data_packages}
    ${src_common}
    ${src_pour}
    ${src}
    )

target_link_libraries(pour PRIVATE lua)

if(NOT MSVC)
    set_target_properties(pour PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
