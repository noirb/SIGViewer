@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %X3D_ROOT_PATH% (
    echo X3D_ROOT_PATH path does not exist!
    echo Expected it to be at: %X3D_ROOT_PATH%
    goto error
)

setlocal
set SCRIPT_DIR = %cd%

:Build
echo Building X3D...
cd %X3D_ROOT_PATH%\parser\cpp

rem Ensure solution is compatible with our version of VS
devenv /Upgrade SgvX3D.sln

rem Build X3D lib
devenv SgvX3D.sln /Build "Release|Win32" /Project libSgvX3D

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
cd %SCRIPT_DIR%
exit /B 1

:end
echo Done building X3D!
cd %SCRIPT_DIR%
