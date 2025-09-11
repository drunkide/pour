
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

if(WIN32)
    # This sometimes breaks on toolchains
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
endif()

######################################################################################################################
# Some useful global variables

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX TRUE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(GCC TRUE)
endif()

if(NOT CMAKE_SIZEOF_VOID_P)
    message(FATAL_ERROR "CMAKE_SIZEOF_VOID_P is not set.")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CPU32 TRUE)
    set(CPU64 FALSE)
else()
    set(CPU32 FALSE)
    set(CPU64 TRUE)
    add_definitions(-DCPU64)
endif()

######################################################################################################################
# Adjust flags for compilers we support

if(MSVC)
    # Force string pooling
    if(NOT OLD_MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GF")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GF")
    endif()

    # Disable noisy warnings
    add_definitions(
        /D_CRT_SECURE_NO_WARNINGS=1
        )
endif()

if(OLD_MSVC)
    string(REPLACE "/GZ" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    if(OLD_MSVC20)
        string(REGEX REPLACE "/Zm[0-9]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REGEX REPLACE "/MDd" "/MD" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "X86" "IX86" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REPLACE "X86" "IX86" CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS}")
        string(REPLACE "X86" "IX86" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    endif()
    string(REPLACE "/pdbtype:sept" "" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/pdbtype:sept" "" CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    string(REPLACE "/pdbtype:sept" "" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/pdbtype:sept" "" CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
endif()

if(GCC OR CLANG)
    add_compile_options(-fno-ident)
    # strip binaries in non-debug builds
    if(NOT MSVC AND NOT BORLAND AND NOT WATCOM
            AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-s")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-s")
    endif()
endif()

######################################################################################################################
# Macros for compiler options

macro(_choose_visibility _func)
    set(_vis "PRIVATE")
    if(_opt_PUBLIC OR _opt_PRIVATE OR _opt_INTERFACE)
        if(NOT _opt_TARGET)
            message(FATAL_ERROR "${_func}: PUBLIC/PRIVATE/INTERFACE can only be specified with TARGET.")
        elseif(_opt_PUBLIC)
            if(_opt_PRIVATE OR _opt_INTERFACE)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "PUBLIC")
        elseif(_opt_PRIVATE)
            if(_opt_PUBLIC OR _opt_INTERFACE)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "PRIVATE")
        elseif(_opt_INTERFACE)
            if(_opt_PRIVATE OR _opt_PUBLIC)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "INTERFACE")
        endif()
    endif()
endmacro()

macro(_report_unparsed_arguments _func)
    foreach(_arg ${_opt_UNPARSED_ARGUMENTS})
        message(FATAL_ERROR "${_func}: unexpected argument \"${_arg}\".")
    endforeach()
endmacro()

#
# Add extra compiler options
#
#   extra_compile_options([TARGET target [PUBLIC/PRIVATE/INTERFACE]] [C/CXX [CHECK]] options)
#
#   if C or CXX are specified, then options will be set only for the specified language
#   if CHECK is additionally specified, CheckCCompilerFlag/CheckCXXCompilerFlag will be used before applying the flag
#
#   options can be additionally prefixed with:
#     ALL       all compilers (default)
#     GCC       GCC only
#     GCC_CLANG GCC and Clang only
#     CLANG     Clang only
#     MINGW     GCC/Clang on Windows only
#     MSVC      MS Visual C++ only
#     BORLAND   Borland only
#     WATCOM    Watcom only
#
function(extra_compile_options)
    set(_single TARGET)
    set(_multi GCC_CLANG GCC CLANG MINGW MSVC BORLAND WATCOM)
    set(_options PUBLIC PRIVATE INTERFACE C CXX CHECK)
    cmake_parse_arguments(_opt "${_options}" "${_single}" "${_multi}" ${ARGN})

    _choose_visibility("extra_compile_options")

    if(_opt_C AND _opt_CXX)
        unset(_opt_C)
        unset(_opt_CXX)
    endif()
    if(_opt_CHECK AND NOT _opt_C AND NOT _opt_CXX)
        message(FATAL_ERROR "extra_compile_options: CHECK can only be specified with one of C or CXX.")
    endif()

    macro(_extra_compile_options_add)
        foreach(_arg ${ARGN})
            if(_opt_C)
                if(_opt_CHECK)
                    check_c_compiler_flag("${_arg}" "HAVE_${_arg}")
                    if(NOT HAVE_${_arg})
                        continue()
                    endif()
                endif()
                set(_arg "$<$<COMPILE_LANGUAGE:C>:${_arg}>")
            endif()
            if(_opt_CXX)
                if(_opt_CHECK)
                    check_cxx_compiler_flag("${_arg}" "HAVE_${_arg}")
                    if(NOT HAVE_${_arg})
                        continue()
                    endif()
                endif()
                set(_arg "$<$<COMPILE_LANGUAGE:CXX>:${_arg}>")
            endif()
            if(_opt_TARGET)
                target_compile_options("${_opt_TARGET}" ${_vis} "${_arg}")
            else()
                add_compile_options("${_arg}")
            endif()
        endforeach()
    endmacro()

    _extra_compile_options_add(${_opt_UNPARSED_ARGUMENTS} ${_opt_ALL})

    if(GCC OR CLANG)
        _extra_compile_options_add(${_opt_GCC_CLANG})
    endif()
    if(GCC)
        _extra_compile_options_add(${_opt_GCC})
    endif()
    if(CLANG)
        _extra_compile_options_add(${_opt_CLANG})
    endif()
    if(MINGW)
        _extra_compile_options_add(${_opt_MINGW})
    endif()
    if(MSVC)
        _extra_compile_options_add(${_opt_MSVC})
    endif()
    if(BORLAND)
        _extra_compile_options_add(${_opt_BORLAND})
    endif()
    if(WATCOM)
        _extra_compile_options_add(${_opt_WATCOM})
    endif()
endfunction()

#
# Add extra linker options
#
#   extra_linker_options([TARGET target] [DEBUG|NODEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL] options)
#
#   options can be additionally prefixed with:
#     ALL       all compilers (default)
#     GCC       GCC only
#     GCC_CLANG GCC and Clang only
#     CLANG     Clang only
#     MINGW     GCC/Clang on Windows only
#     MSVC      MS Visual C++ only
#     BORLAND   Borland only
#     WATCOM    Watcom only
#
function(extra_linker_options)
    set(_single TARGET)
    set(_multi GCC_CLANG GCC CLANG MINGW MSVC BORLAND WATCOM)
    set(_options DEBUG NODEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    cmake_parse_arguments(_opt "${_options}" "${_single}" "${_multi}" ${ARGN})

    if(_opt_NODEBUG)
        set(_opt_RELEASE TRUE)
        set(_opt_RELWITHDEBINFO TRUE)
        set(_opt_MINSIZEREL TRUE)
    endif()

    if(_opt_DEBUG AND _opt_RELEASE AND _opt_RELWITHDEBINFO AND _opt_MINSIZEREL)
        set(_suffixes FLAGS)
    elseif(NOT _opt_DEBUG AND NOT _opt_RELEASE AND NOT _opt_RELWITHDEBINFO AND NOT _opt_MINSIZEREL)
        set(_suffixes FLAGS)
    else()
        set(_suffixes)
        if(_opt_DEBUG)
            list(APPEND _suffixes FLAGS_DEBUG)
        endif()
        if(_opt_RELEASE)
            list(APPEND _suffixes FLAGS_RELEASE)
        endif()
        if(_opt_RELWITHDEBINFO)
            list(APPEND _suffixes FLAGS_RELWITHDEBINFO)
        endif()
        if(_opt_MINSIZEREL)
            list(APPEND _suffixes FLAGS_MINSIZEREL)
        endif()
    endif()

    macro(_extra_linker_options_add)
        foreach(_arg ${ARGN})
            foreach(_suffix ${_suffixes})
                if(_opt_TARGET)
                    set_property(TARGET "${_opt_TARGET}" APPEND_STRING PROPERTY LINK_${_suffix} " ${_arg}")
                else()
                    set(CMAKE_EXE_LINKER_${_suffix} "${CMAKE_EXE_LINKER_${_suffix}} ${_arg}")
                    set(CMAKE_SHARED_LINKER_${_suffix} "${CMAKE_SHARED_LINKER_${_suffix}} ${_arg}")
                    set(CMAKE_EXE_LINKER_${_suffix} "${CMAKE_EXE_LINKER_${_suffix}}" PARENT_SCOPE)
                    set(CMAKE_SHARED_LINKER_${_suffix} "${CMAKE_SHARED_LINKER_${_suffix}}" PARENT_SCOPE)
                endif()
            endforeach()
        endforeach()
    endmacro()

    _extra_linker_options_add(${_opt_UNPARSED_ARGUMENTS} ${_opt_ALL})

    if(GCC OR CLANG)
        _extra_linker_options_add(${_opt_GCC_CLANG})
    endif()
    if(GCC)
        _extra_linker_options_add(${_opt_GCC})
    endif()
    if(CLANG)
        _extra_linker_options_add(${_opt_CLANG})
    endif()
    if(MINGW)
        _extra_linker_options_add(${_opt_MINGW})
    endif()
    if(MSVC)
        _extra_linker_options_add(${_opt_MSVC})
    endif()
    if(BORLAND)
        _extra_linker_options_add(${_opt_BORLAND})
    endif()
    if(WATCOM)
        _extra_linker_options_add(${_opt_WATCOM})
    endif()
endfunction()

#
# Ask C compiler to enforce C89 limitations (currently only GCC and Clang)
#
#   force_c89() - globally for everything after this call
#   force_c89(TARGET target) - specifically for target <target>
#
function(force_c89)
    set(_options)
    set(_multi)
    set(_single TARGET)
    cmake_parse_arguments(_opt "${_options}" "${_single}" "${_multi}" ${ARGN})

    _report_unparsed_arguments("force_c89")

    if(_opt_TARGET)
        extra_compile_options(TARGET "${_opt_TARGET}" PRIVATE GCC_CLANG -ansi)
    else()
        extra_compile_options(GCC_CLANG -ansi)
    endif()
endfunction()

#
# Enable maximum warnings for the compiler
#
#   enable_maximum_warnings() - globally for everything after this call
#   enable_maximum_warnings(TARGET target) - specifically for target <target>
#
function(enable_maximum_warnings)
    set(_options)
    set(_multi)
    set(_single TARGET)
    cmake_parse_arguments(_opt "${_options}" "${_single}" "${_multi}" ${ARGN})

    _report_unparsed_arguments("enable_maximum_warnings")

    set(_prefix)
    if(_opt_TARGET)
        set(_prefix TARGET ${_opt_TARGET})
    endif()

    if(MSVC AND NOT OLD_MSVC)
        # avoid stupid warning "/W3 was overriden by /W4"
        string(REPLACE "/W3" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        string(REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    endif()

    extra_compile_options(${_prefix}
        WATCOM
            -w4
        BORLAND
            -w-   -wamb -wamp -wasm -waus -wbbf -wbei -wbig -wccc -wcln -wcpt -wdef -wdpu -wdsz -wdup
            -weas -weff -wext -whch -whid -wibc -will -winl -wlin -wlvc -wmpc -wmpd -wmsg -wnak -wncf
            -wnci -wnod -wnsf -wnst -wntd -wnvf -wobi -wobs -wofp -wovl -wpar -wpch -wpia -wpin -wpre
            -wpro -wrch -wret -wrng -wrpt -wrvl -wsig -wstv -wsus -wucp -wuse -wvoi -wxxx -wzdi#-wstu
        GCC_CLANG
            -pedantic
            -Wall
            -Wconversion
            -Wshadow
        )

    if(MSVC AND NOT OLD_MSVC)
        extra_compile_options(${_prefix}
            /W4
            /Wall
            /wd4324     # structure was padded due to alignment
            /wd4710     # function not inlined
            /wd4711     # function selected for automatic inline expansion
            /wd4738     # storing 32-bit float result in memory, possible loss of performance
            /wd4820     # padding added after data member
            /wd5045     # compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
            )
    endif()

    if((GCC OR CLANG) AND NOT EGCS)
        extra_compile_options(${_prefix}
            -Wextra
            )
    endif()

    if(EMSCRIPTEN)
        extra_compile_options(${_prefix}
            -Wno-c99-extensions
            -Wno-gnu-zero-variadic-macro-arguments
            -Wno-dollar-in-identifier-extension
            )
    endif()
endfunction()

#
# Adds -fvisibility=hidden for compilers that are known to support it
#
function(gcc_visibility_hidden)
    if(CLANG)
        add_definitions(-fvisibility=hidden)
    elseif(GCC AND NOT DJGPP)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.5" OR
                (NOT WIN32 AND CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.0"))
            add_definitions(-fvisibility=hidden)
        endif()
    endif()
endfunction()

#
# Creates source groups and configures compilation for platform-specific source files.
# Input file list should be relative to <directory>.
# List of source files is written into <output> variable for later use.
#
#   create_source_list(output directory
#       [GROUP name]
#       [SOURCES] sources
#       [HEADERS headers]
#       [HTML5_SOURCES sources]
#       [LINUX_SOURCES sources]
#       [MSDOS_SOURCES sources]
#       [MSWIN_SOURCES sources]
#   )
#
macro(create_source_list _output _directory)
    set(_options)
    set(_single GROUP)
    set(_multi SOURCES HEADERS HTML5_SOURCES LINUX_SOURCES MSDOS_SOURCES MSWIN_SOURCES)
    cmake_parse_arguments(_opt "${_options}" "${_single}" "${_multi}" ${ARGN})

    if(NOT _opt_GROUP)
        set(_opt_GROUP "Source Files")
    endif()

    foreach(_file
            ${_opt_UNPARSED_ARGUMENTS}
            ${_opt_SOURCES}
            ${_opt_HEADERS}
            ${_opt_HTML5_SOURCES}
            ${_opt_LINUX_SOURCES}
            ${_opt_MSDOS_SOURCES}
            ${_opt_MSWIN_SOURCES}
            )
        get_filename_component(_dir "${_file}" DIRECTORY)
        string(REPLACE "/" "\\" _dir "${_opt_GROUP}/${_dir}")
        source_group("${_dir}" FILES "${_directory}/${_file}")
        list(APPEND ${_output} "${_directory}/${_file}")
    endforeach()

    macro(_exclude_sources var)
        foreach(_file ${${var}})
            set_source_files_properties("${_directory}/${_file}"
                PROPERTIES HEADER_FILE_ONLY TRUE)
        endforeach()
    endmacro()

    _exclude_sources(_opt_HEADERS)

    if(_opt_HTML5_SOURCES AND NOT EMSCRIPTEN)
        _exclude_sources(_opt_HTML5_SOURCES)
    endif()
    if(_opt_LINUX_SOURCES AND NOT LINUX)
        _exclude_sources(_opt_LINUX_SOURCES)
    endif()
    if(_opt_MSDOS_SOURCES AND NOT MSDOS)
        _exclude_sources(_opt_MSDOS_SOURCES)
    endif()
    if(_opt_MSWIN_SOURCES AND NOT WIN32)
        _exclude_sources(_opt_MSWIN_SOURCES)
    endif()
endmacro()
