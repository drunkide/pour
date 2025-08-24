@echo off
if exist "%~dp0win32-clang-4.0.0\bin\clang.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-clang-400 "%~dp0win32-clang-4.0.0" || exit /B 1
:installed
