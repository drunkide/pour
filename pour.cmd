@echo off
call "%~dp0build_mingw.cmd" || exit /B 1
set "dir=%~dp0"

setlocal enabledelayedexpansion
echo ====\\
set "ARGS="
set i=0
:loop
if "%~1"=="" goto afterloop
set /a i+=1
set "arg=%~1"
echo     :: %arg%
if "%arg:~-1%"=="\" set "arg=%arg%\"
set "ARGS=!ARGS! "%arg%""
shift
goto loop
:afterloop
echo ====//

"%dir%build\mingw32\pour" !ARGS! || exit /B 1
