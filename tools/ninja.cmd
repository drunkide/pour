@echo off
setlocal
set "PATH=%~dp0win32-ninja;%PATH%"
if exist "%~dp0win32-ninja\ninja.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-ninja "%~dp0win32-ninja" || exit /B 1
:installed
"%~dp0win32-ninja\ninja.exe" %* || exit /B 1
