@echo off
setlocal
set "PATH=%~dp0tools\win32-mingw440-32\bin;%PATH%"
call "%~dp0tools\mingw440-32.cmd" || exit /B 1

cd "%~dp0" || exit /B 1

if not exist build mkdir build
if not exist build\mingw32 mkdir build\mingw32
cd build\mingw32 || exit /B 1

if exist pour.exe goto skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    "%~dp0" ^
    || exit /B 1

:skip_cmake

mingw32-make %* || exit /B 1
cd "%~dp0" || exit /B 1
