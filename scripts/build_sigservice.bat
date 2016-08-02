@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %SIGSERVICE_ROOT_PATH% (
    echo SIGSERVICE_ROOT_PATH path does not exist!
    echo Expected it to be at: %SIGSERVICE_ROOT_PATH%
    goto error
)

setlocal
set SCRIPT_DIR = %cd%

:Build
echo Building SIGService...
cd %SIGSERVICE_ROOT_PATH%\Windows

rem Ensure solution is compatible with whichever version of VS we're using
devenv /Upgrade SIGService_2010.sln

rem build the SIGService lib
devenv SIGService_2010.sln /Build "Release|Win32" /Project SIGService

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
cd %SCRIPT_DIR%
exit /B 1

:end
cd %SCRIPT_DIR%
echo Done building SIGService!
