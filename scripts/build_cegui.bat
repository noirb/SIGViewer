@echo off

echo Setting up build environment
call .\setenv.bat

if NOT EXIST %CEGUI_ROOT_PATH% (
    echo CEGUI path does not exist!
    echo Expected it to be at: %CEGUI_ROOT_PATH%
    goto error
)
if NOT EXIST %CEGUI_DEPS_ROOT% (
    echo CEGUI Deps path does not exist!
    echo Expected it to be at: %CEGUI_DEPS_ROOT%
    goto error
)
if NOT EXIST %BOOST_ROOT% (
    echo BOOST_ROOT path does not exist!
    echo Expected it to be at: %BOOST_ROOT%
    echo Please ensure Boost is set up before trying to build CEGUI!
    goto error
)

setlocal
set SCRIPT_DIR = %cd%

:BuildDeps
echo Building CEGUI deps...
cd %CEGUI_DEPS_ROOT%

if NOT EXIST .\build mkdir build
cd build

%CMAKE% -G %VS_VERSION% ..
devenv /Build "Release|Win32" CEGUI-DEPS.sln

if ERRORLEVEL 1 goto error

:CopyDeps
echo Copying CEGUI Dependencies to CEGUI root
if NOT EXIST %CEGUI_DEPS_ROOT%\build\dependencies (
    echo CEGUI Dependencies not found in: %CEGUI_DEPS_ROOT%\build\dependencies
    echo CEGUI Dependencies MUST be built first!
    goto error
)
echo Source Dir: %CEGUI_DEPS_ROOT%\build\dependencies
echo Dest Dir:   %CEGUI_ROOT_PATH%\dependencies
xcopy %CEGUI_DEPS_ROOT%\build\dependencies %CEGUI_ROOT_PATH%\dependencies\ /E /Y

:BuildCEGUI
echo Building CEGUI...
cd %CEGUI_ROOT_PATH%

%CMAKE% -G %VS_VERSION% -D CEGUI_BUILD_RENDERER_OGRE=true -D OGRE_LIB=%OGRE_SDK%\lib\Release\OgreMain.lib -D OGRE_H_PATH=%OGRE_SDK%\include\OGRE -D OGRE_H_BUILD_SETTINGS_PATH=%OGRE_SDK%\include\Ogre -D OIS_LIB=%OGRE_SDK%\lib\Release\OIS.lib -D OIS_H_PATH=%OGRE_SDK%\include\OIS -D Boost_INCLUDE_DIR=%BOOST_ROOT% -D Boost_LIBRARY_DIR=%BOOST_ROOT%\stage\lib .
devenv /Build "Release|Win32" CEGUI.sln

if ERRORLEVEL 1 goto error
if ERRORLEVEL 0 goto end

:error
cd %SCRIPT_DIR%
echo ERROR: One or more build steps failed! Exiting...
exit /B 1

:end
cd %SCRIPT_DIR%
echo Done building CEGUI!
echo Binaries are located at: %CEGUI_ROOT_PATH%\build\bin
