@echo off
setlocal
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0
if exist "%POUR_PACKAGE_DIR%\win32-clang-4.0.0\bin\clang.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-clang-400 "%POUR_PACKAGE_DIR%\win32-clang-4.0.0" || exit /B 1
:installed
