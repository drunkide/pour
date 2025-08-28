@echo off
setlocal
cd "%~dp0" || exit /B 1

if exist build\msvc_posix\pour.sln goto skip_cmake

if not exist build mkdir build
if not exist build\msvc_posix mkdir build\msvc_posix
cd build\msvc_posix || exit /B 1

call "%~dp0tools\cmake-3.31.4.cmd" ^
    -DUSE_POSIX_IO=TRUE ^
    "%~dp0" ^
    || exit /B 1

:skip_cmake

cd "%~dp0build\msvc_posix" || exit /B 1

call "%~dp0tools\cmake-3.31.4.cmd" ^
    --build . ^
    || exit /B 1
