
@echo off
rem --------------------------------------------------------
rem This is a script-generated file! Edit at your own risk!
rem --------------------------------------------------------
echo Checking for JDK path...
call .\find_jdk.bat
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set VS_VERSION="Visual Studio 11 2012"
set SIGSERVICE_ROOT_PATH="K:\dev\SIGVerse\test\SIGService-master"
if not exist %SIGSERVICE_ROOT_PATH% (
  echo SIGSERVICE_ROOT_PATH directory could not be found: %SIGSERVICE_ROOT_PATH%
  goto error
)
set LIBOVR_ROOT_PATH="K:\dev\SIGVerse\test\extern\OculusSDK\LibOVR"
if not exist %LIBOVR_ROOT_PATH% (
  echo LIBOVR_ROOT_PATH directory could not be found: %LIBOVR_ROOT_PATH%
  goto error
)
set BOOST_ROOT="K:\dev\SIGVerse\test\extern\boost_1_55_0"
if not exist %BOOST_ROOT% (
  echo BOOST_ROOT directory could not be found: %BOOST_ROOT%
  goto error
)
set OGRE_SDK="K:\dev\SIGVerse\test\extern\OgreSDK_vc11_v1-9-0"
if not exist %OGRE_SDK% (
  echo OGRE_SDK directory could not be found: %OGRE_SDK%
  goto error
)
set CEGUI_ROOT_PATH="K:\dev\SIGVerse\test\extern\cegui-0.8.7"
if not exist %CEGUI_ROOT_PATH% (
  echo CEGUI_ROOT_PATH directory could not be found: %CEGUI_ROOT_PATH%
  goto error
)
set X3D_ROOT_PATH="K:\dev\SIGVerse\test\x3d-master"
if not exist %X3D_ROOT_PATH% (
  echo X3D_ROOT_PATH directory could not be found: %X3D_ROOT_PATH%
  goto error
)
set CEGUI_DEPS_ROOT="K:\dev\SIGVerse\test\extern\cegui-deps-0.8.x-src"
if not exist %CEGUI_DEPS_ROOT% (
  echo CEGUI_DEPS_ROOT directory could not be found: %CEGUI_DEPS_ROOT%
  goto error
)
set VS_TOOLS_PATH="C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\Tools\"
if not exist %VS_TOOLS_PATH% (
  echo VS_TOOLS_PATH directory could not be found: %VS_TOOLS_PATH%
  goto error
)
set OPENSSL_ROOT_DIR="K:\dev\SIGVerse\test\extern\openssl-0.9.8k_WIN32"
if not exist %OPENSSL_ROOT_DIR% (
  echo OPENSSL_ROOT_DIR directory could not be found: %OPENSSL_ROOT_DIR%
  goto error
)
set LIBSSH2_ROOT_PATH="K:\dev\SIGVerse\test\extern\libssh2-1.7.0"
if not exist %LIBSSH2_ROOT_PATH% (
  echo LIBSSH2_ROOT_PATH directory could not be found: %LIBSSH2_ROOT_PATH%
  goto error
)
set BUILD_SIGSERVICE_INC=K:\dev\SIGVerse\test\SIGService-master\Windows\SIGService
set BUILD_X3D_INC=K:\dev\SIGVerse\test\x3d-master\parser\cpp\X3DParser
set BUILD_OGRE_INC=K:\dev\SIGVerse\test\extern\OgreSDK_vc11_v1-9-0\include
set BUILD_BOOST_INC=K:\dev\SIGVerse\test\extern\boost_1_55_0
set BUILD_CEGUI_INC=K:\dev\SIGVerse\test\extern\cegui-0.8.7\cegui\include
set BUILD_LIBSSH2_INC=K:\dev\SIGVerse\test\extern\libssh2-1.7.0\include
set BUILD_OPENSSL_INC=K:\dev\SIGVerse\test\extern\openssl-0.9.8k_WIN32\include
set BUILD_LIBOVR_INC=K:\dev\SIGVerse\test\extern\OculusSDK\LibOVR\Include
set BUILD_SIGSERVICE_LIB=K:\dev\SIGVerse\test\SIGService-master\Windows\Release_2010
set BUILD_X3D_LIB=K:\dev\SIGVerse\test\x3d-master\parser\cpp\Release
set BUILD_OGRE_LIB=K:\dev\SIGVerse\test\extern\OgreSDK_vc11_v1-9-0\lib
set BUILD_BOOST_LIB=K:\dev\SIGVerse\test\extern\boost_1_55_0\stage\lib
set BUILD_CEGUI_LIB=K:\dev\SIGVerse\test\extern\cegui-0.8.7\lib
set BUILD_LIBSSH2_LIB=K:\dev\SIGVerse\test\extern\libssh2-1.7.0\win32\Release_dll
set BUILD_OPENSSL_LIB=K:\dev\SIGVerse\test\extern\openssl-0.9.8k_WIN32\lib
set BUILD_ZLIB_LIB=K:\dev\SIGVerse\test\extern\cegui-0.8.7\dependencies\lib\static
set BUILD_LIBOVR_LIB=K:\dev\SIGVerse\test\extern\OculusSDK\LibOVR\Lib\Win32\VS2012
call %VS_TOOLS_PATH%\VsDevCmd.bat
if errorlevel 0 goto end
:error
exit /B 1
:end
