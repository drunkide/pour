
set(src
    _entry.c
    main.c
    main.h
    pour.h
    script.c
    script.h
    )

source_group("Source Files" FILES ${src})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
    add_definitions(-Wall -Wextra -Werror)
endif()

add_executable(pour ${src})
target_link_libraries(pour PRIVATE lua)

if(NOT MSVC)
    set_target_properties(pour PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
