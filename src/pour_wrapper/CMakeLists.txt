
if(NOT MSVC)
    add_definitions(-ansi -pedantic -Wall -Wextra -Wshadow -Wconversion -Werror)
endif()

if(WIN32)

    if(MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:entry")
        string(REGEX REPLACE "/RTC[0-9a-zA-Z]*" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        add_definitions(/GS-)
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nodefaultlibs -nostartfiles")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/pour_wrapper_windows.ld")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--nostdlib -Wl,--entry,_entry")
    endif()

    add_definitions(
        -DUNICODE
        -D_UNICODE
        )

    set(src
        pour_wrapper_windows.c
        pour_wrapper_windows.ld
        )

    source_group("Source Files" FILES ${src})

    add_executable(pour_wrapper_windows EXCLUDE_FROM_ALL
        ${src}
        )

    set_source_files_properties(
        pour_wrapper_windows.c
        PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pour_wrapper_windows.ld"
        )

    set_target_properties(pour_wrapper_windows PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

endif()
