@echo off
setlocal
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0
set "PATH=%POUR_PACKAGE_DIR%\win32-cmake-3.31.4\bin;%PATH%"
if exist "%POUR_PACKAGE_DIR%\win32-cmake-3.31.4\bin\cmake.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-cmake-3.31.4 "%POUR_PACKAGE_DIR%\win32-cmake-3.31.4" || exit /B 1
:installed
"%POUR_PACKAGE_DIR%\win32-cmake-3.31.4\bin\cmake.exe" %* || exit /B 1
