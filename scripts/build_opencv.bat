@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %OPENCV_ROOT% (
    echo OPENCV_ROOT path does not exist!
    echo Expected it to be at: %CEGUI_ROOT_PATH%
    goto error
)
if NOT EXIST %BOOST_ROOT% (
    echo BOOST_ROOT path does not exist!
    echo Expected it to be at: %BOOST_ROOT%
    echo Please ensure Boost is set up before trying to build OpenCV!
    goto error
)

setlocal
set SCRIPT_DIR = %cd%

:BuildOpenCV
echo Building OpenCV...
cd %OPENCV_ROOT%\build

%CMAKE% -G %VS_VERSION% ..\sources
devenv /Build "Release|Win32" OpenCV.sln

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
cd %SCRIPT_DIR%
echo ERROR: One or more build steps failed! Exiting...
exit /B 1

:end
cd %SCRIPT_DIR%
echo Done building OpenCV!
echo Binaries are located at: %OPENCV_ROOT%\build\
