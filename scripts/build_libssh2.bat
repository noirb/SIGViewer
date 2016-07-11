@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %LIBSSH2_ROOT_PATH% (
    echo LIBSSH2_ROOT_PATH path does not exist!
    echo Expected it to be at: %LIBSSH2_ROOT_PATH%
    goto error
)


:Build
echo Building LIBSSH2...
cd %LIBSSH2_ROOT_PATH%\win32

rem Make an .sln
devenv /Upgrade libssh2.dsw

rem Build just the libssh2 project
set INCLUDE=%BUILD_OPENSSL_INC%;%INCLUDE%
set LIB=%BUILD_OPENSSL_LIB%;%BUILD_ZLIB_LIB%;%LIB%
devenv libssh2.sln /Build "OpenSSL DLL Release" /Project libssh2 /UseEnv 

if ERRORLEVEL 1 goto error

rem devenv /Build "Release|Win32" libssh2.sln

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
exit /B 1

:end
echo Done building LIBSSH2!
