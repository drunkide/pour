@echo off
call build_clang.cmd || exit /B 1
call build_clang_posix.cmd || exit /B 1
call build_mingw.cmd || exit /B 1
call build_mingw_wrapper.cmd || exit /B 1
call build_mingw64.cmd || exit /B 1
call build_mingw64_posix.cmd || exit /B 1
call build_msvc.cmd || exit /B 1
call build_msvc_posix.cmd || exit /B 1
