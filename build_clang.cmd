@echo off
setlocal
set "PATH=%~dp0tools\win32-clang-4.0.0\bin;%PATH%"
call "%~dp0tools\clang-4.0.0.cmd" || exit /B 1

cd "%~dp0" || exit /B 1

if not exist build mkdir build
if not exist build\clang mkdir build\clang
cd build\clang || exit /B 1

if exist pour.exe goto skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_SYSTEM_NAME=Windows-GNU ^
    -DCMAKE_C_COMPILER=clang ^
    -DCMAKE_C_COMPILER_WORKS=TRUE ^
    -DCMAKE_CXX_COMPILER=clang ^
    -DCMAKE_CXX_COMPILER_WORKS=TRUE ^
    -DCMAKE_MAKE_PROGRAM="%~dp0tools\ninja.cmd" ^
    -DCMAKE_AR="%~dp0tools\win32-clang-4.0.0\bin\llvm-ar" ^
    -DCMAKE_RANLIB="%~dp0tools\win32-clang-4.0.0\bin\llvm-ranlib" ^
    -DCMAKE_C_FLAGS="-target i686-w64-mingw32" ^
    -DWIN32=TRUE ^
    "%~dp0" ^
    || exit /B 1

:skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    --build . ^
    || exit /B 1 ^

cd "%~dp0" || exit /B 1
