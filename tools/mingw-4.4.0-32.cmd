@echo off
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0
if exist "%POUR_PACKAGE_DIR%\win32-mingw440-32\bin\gcc.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-mingw440-32 "%POUR_PACKAGE_DIR%\win32-mingw440-32" || exit /B 1
:installed
