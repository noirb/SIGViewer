@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %LIBSSH2_ROOT_PATH% (
    echo LIBSSH2_ROOT_PATH path does not exist!
    echo Expected it to be at: %LIBSSH2_ROOT_PATH%
    goto error
)

setlocal
set SCRIPT_DIR = %cd%

:Build
echo Building LIBSSH2...
cd %LIBSSH2_ROOT_PATH%

if NOT EXIST build (
mkdir build
)

cd build

%CMAKE% .. -G %VS_VERSION% -DBUILD_EXAMPLES=false -DBUILD_TESTING=false

devenv /Build "Release|Win32" libssh2.sln

if ERRORLEVEL 1 goto error

rem devenv /Build "Release|Win32" libssh2.sln

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
cd %SCRIPT_DIR%
exit /B 1

:end
cd %SCRIPT_DIR%
echo Done building LIBSSH2!
