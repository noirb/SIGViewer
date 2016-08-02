@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %BOOST_ROOT% (
    echo BOOST_ROOT path does not exist!
    echo Expected it to be at: %BOOST_ROOT%
    goto error
)

setlocal 
set SCRIPT_DIR = %cd%

rem determine which compiler to tell Boost to use
if %VS_VERSION% == "Visual Studio 14 2015" (
    set BOOST_VS_TOOLSET=msvc-14.0
)
if %VS_VERSION% == "Visual Studio 12 2013" (
    set BOOST_VS_TOOSET=msvc-12.0
)
if %VS_VERSION% == "Visual Studio 11 2012" (
    set BOOST_VS_TOOLSET=msvc-11.0
)
if %VS_VERSION% == "Visual Studio 10 2010" (
    set BOOST_VS_TOOLSET=msvc-10.0
)
if %VS_VERSION% == "Visual Studio 9 2008" (
    set BOOST_VS_TOOLSET=msvc-9.0
)


echo Using toolset: %BOOST_VS_TOOLSET%

:Boostrap
echo Configuring BOOST...
cd %BOOST_ROOT%

call bootstrap.bat
if ERRORLEVEL 1 goto error

:Build
echo Building BOOST libraries...
bjam toolset=%BOOST_VS_TOOLSET% --build-type=complete --with-thread --with-system --with-timer --with-chrono --with-date_time

if ERRORLEVEL 1 goto error

if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
cd %SCRIPT_DIR%
exit /B 1

:end
cd %SCRIPT_DIR%
echo Done building BOOST!
