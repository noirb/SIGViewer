@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %BOOST_ROOT% (
    echo BOOST_ROOT path does not exist!
    echo Expected it to be at: %BOOST_ROOT%
    goto error
)


:Boostrap
echo Configuring BOOST...
cd %BOOST_ROOT%

call bootstrap.bat
if ERRORLEVEL 1 goto error

:Build
echo Building BOOST libraries...
bjam toolset=msvc-11.0 --build-type=complete --with-thread --with-system --with-timer --with-chrono --with-date_time
if ERRORLEVEL 1 goto error

if ERRORLEVEL 0 goto end

:error
echo ERROR: One or more build steps failed! Exiting...
exit /B 1

:end
echo Done building BOOST!
