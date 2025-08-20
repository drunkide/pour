
set(src_common
    common/common.h
    common/dirs.c
    common/dirs.h
    common/script.c
    common/script.h
    )

set(src_pour
    pour/main.c
    pour/main.h
    )

set(src
    _main.c
    )

source_group("Source Files\\common" FILES ${src_common})
source_group("Source Files\\pour" FILES ${src_pour})
source_group("Source Files" FILES ${src})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    add_definitions(-Wall -Wextra -Werror)
endif()

add_executable(pour
    ${src_common}
    ${src_pour}
    ${src}
    )

target_link_libraries(pour PRIVATE lua)

if(NOT MSVC)
    set_target_properties(pour PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
