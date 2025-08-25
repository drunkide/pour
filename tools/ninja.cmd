@echo off
setlocal
if "%POUR_PACKAGE_DIR%" == "" set POUR_PACKAGE_DIR=%~dp0
set "PATH=%POUR_PACKAGE_DIR%\win32-ninja;%PATH%"
if exist "%POUR_PACKAGE_DIR%\win32-ninja\ninja.exe" goto installed
git clone https://github.com/thirdpartystuff/win32-ninja "%POUR_PACKAGE_DIR%\win32-ninja" || exit /B 1
:installed
"%POUR_PACKAGE_DIR%\win32-ninja\ninja.exe" %* || exit /B 1
