@echo off
setlocal
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0tools
set "PATH=%POUR_PACKAGE_DIR%\win32-mingw440-32\bin;%PATH%"
call "%~dp0tools\mingw-4.4.0-32.cmd" || exit /B 1

cd "%~dp0" || exit /B 1

if not exist build mkdir build
if not exist build\mingw32_minsizerel mkdir build\mingw32_minsizerel
cd build\mingw32_minsizerel || exit /B 1

if exist pour.exe goto skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=MinSizeRel ^
    "%~dp0" ^
    || exit /B 1

:skip_cmake

mingw32-make pour_wrapper_windows || exit /B 1
cd "%~dp0" || exit /B 1
