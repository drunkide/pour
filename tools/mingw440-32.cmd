@echo off
if exist "%~dp0win32-mingw440-32\bin\gcc.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-mingw440-32 "%~dp0win32-mingw440-32" || exit /B 1
:installed
