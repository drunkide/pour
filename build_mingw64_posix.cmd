@echo off
setlocal
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0tools
set "PATH=%POUR_PACKAGE_DIR%\win32-mingw810-64\bin;%PATH%"
call "%~dp0tools\mingw-8.1.0-64.cmd" || exit /B 1

cd "%~dp0" || exit /B 1

if not exist build mkdir build
if not exist build\mingw64_posix mkdir build\mingw64_posix
cd build\mingw64_posix || exit /B 1

if exist pour.exe goto skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    -G "Ninja" ^
    -DCMAKE_MAKE_PROGRAM="%~dp0tools\ninja.cmd" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DUSE_POSIX_IO=TRUE ^
    "%~dp0" ^
    || exit /B 1

:skip_cmake

call "%~dp0tools\cmake-3.5.2.cmd" ^
    --build . ^
    || exit /B 1

cd "%~dp0" || exit /B 1
