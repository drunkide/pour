
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

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CPU32 TRUE)
    set(CPU64 FALSE)
else()
    set(CPU32 FALSE)
    set(CPU64 TRUE)
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
endif()

######################################################################################################################
# Macros for compiler options

macro(_choose_visibility _func)
    set(_vis "PRIVATE")
    if(opt_PUBLIC OR opt_PRIVATE OR opt_INTERFACE)
        if(NOT opt_TARGET)
            message(FATAL_ERROR "${_func}: PUBLIC/PRIVATE/INTERFACE can only be specified with TARGET.")
        elseif(opt_PUBLIC)
            if(opt_PRIVATE OR opt_INTERFACE)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "PUBLIC")
        elseif(opt_PRIVATE)
            if(opt_PUBLIC OR opt_INTERFACE)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "PRIVATE")
        elseif(opt_INTERFACE)
            if(opt_PRIVATE OR opt_PUBLIC)
                message(FATAL_ERROR "${_func}: multiple conflicting PUBLIC/PRIVATE/INTERFACE options.")
            endif()
            set(_vis "INTERFACE")
        endif()
    endif()
endmacro()

macro(_report_unparsed_arguments _func)
    foreach(_arg ${opt_UNPARSED_ARGUMENTS})
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
    set(single TARGET)
    set(multi GCC_CLANG GCC CLANG MINGW MSVC BORLAND WATCOM)
    set(options PUBLIC PRIVATE INTERFACE C CXX CHECK)
    cmake_parse_arguments(opt "${options}" "${single}" "${multi}" ${ARGN})

    _choose_visibility("extra_compile_options")

    if(opt_C AND opt_CXX)
        unset(opt_C)
        unset(opt_CXX)
    endif()
    if(opt_CHECK AND NOT opt_C AND NOT opt_CXX)
        message(FATAL_ERROR "extra_compile_options: CHECK can only be specified with one of C or CXX.")
    endif()

    macro(_extra_compile_options_add)
        foreach(_arg ${ARGN})
            if(opt_C)
                if(opt_CHECK)
                    check_c_compiler_flag("${_arg}" "HAVE_${_arg}")
                    if(NOT HAVE_${_arg})
                        continue()
                    endif()
                endif()
                set(_arg "$<$<COMPILE_LANGUAGE:C>:${_arg}>")
            endif()
            if(opt_CXX)
                if(opt_CHECK)
                    check_cxx_compiler_flag("${_arg}" "HAVE_${_arg}")
                    if(NOT HAVE_${_arg})
                        continue()
                    endif()
                endif()
                set(_arg "$<$<COMPILE_LANGUAGE:CXX>:${_arg}>")
            endif()
            if(opt_TARGET)
                target_compile_options("${opt_TARGET}" ${_vis} "${_arg}")
            else()
                add_compile_options("${_arg}")
            endif()
        endforeach()
    endmacro()

    _extra_compile_options_add(${opt_UNPARSED_ARGUMENTS} ${opt_ALL})

    if(GCC OR CLANG)
        _extra_compile_options_add(${opt_GCC_CLANG})
    endif()
    if(GCC)
        _extra_compile_options_add(${opt_GCC})
    endif()
    if(CLANG)
        _extra_compile_options_add(${opt_CLANG})
    endif()
    if(MINGW)
        _extra_compile_options_add(${opt_MINGW})
    endif()
    if(MSVC)
        _extra_compile_options_add(${opt_MSVC})
    endif()
    if(BORLAND)
        _extra_compile_options_add(${opt_BORLAND})
    endif()
    if(WATCOM)
        _extra_compile_options_add(${opt_WATCOM})
    endif()
endfunction()

#
# Ask C compiler to enforce C89 limitations (currently only GCC and Clang)
#
#   force_c89() - globally for everything after this call
#   force_c89(TARGET target) - specifically for target <target>
#
function(force_c89)
    set(options)
    set(multi)
    set(single TARGET)
    cmake_parse_arguments(opt "${options}" "${single}" "${multi}" ${ARGN})

    _report_unparsed_arguments("force_c89")

    if(opt_TARGET)
        extra_compile_options(TARGET "${opt_TARGET}" PRIVATE GCC_CLANG -ansi)
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
    set(options)
    set(multi)
    set(single TARGET)
    cmake_parse_arguments(opt "${options}" "${single}" "${multi}" ${ARGN})

    _report_unparsed_arguments("enable_maximum_warnings")

    set(_prefix)
    if(opt_TARGET)
        set(_prefix TARGET ${opt_TARGET})
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
endfunction()
