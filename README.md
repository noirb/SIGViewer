SIGVerse/SIGViewer
=============

How to change normal SIGViewer to oculus SIGViewer.

open:SIGViewer\Release\SIGVerse.ini

change  
OCULUS_MODE=false  
to  
OCULUS_MODE=true  
then you can use oculus rift in SIGVerse!

and if you want fullscreen mode  
change  
FULLSCREEN_MODE=false  
to  
FULLSCREEN_MODE=true


## Dependencies
To build this project and its dependecies, you need:

* [SIGService](https://github.com/SIGVerse/SIGService)
* [X3D Parser](https://github.com/SIGVerse/x3d)
* [The Ogre SDK](http://www.ogre3d.org/download/sdk)
* [CEGUI](http://cegui.org.uk/) (built with Ogre support)
* [Java JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html)
* [Boost](http://www.boost.org/doc/libs/1_55_0/more/getting_started/windows.html)
* [LibSSH2](https://www.libssh2.org/)
* [OpenSSL](https://www.openssl.org/)
* [Zlib](http://www.zlib.net/) (actually included with CEGUI's dependencies, so not necessary to install separately)
* [Oculus SDK](https://developer.oculus.com/downloads/), version 0.4.3
* [CMake](https://cmake.org/download/)
* (recommended) [7-Zip](http://www.7-zip.org/)

A PowerShell script, `setup_env.ps1`, is provided to help build and configure these in order to get you a working development environment as quickly as possible.
Run this script from the directory you want to make your "SIGVerse Root" (e.g. one directory level above the location you cloned SIGViewer to).
It will:

1. Download recent versions of all of the above dependencies, excluding CMake and 7-Zip (which you should install yourself). The versions used at the moment are all compatible with Visual Studio 2012.
2. Unzip them into your "SIGVerse Root" with the following structure:
    `SIGVerseRoot\*        -- SIGVerse projects (SIGViewer, etc.)`
    `SIGVerseRoot\external -- All other dependencies`
3. Compile necessary Boost libraries used by Ogre and CEGUI
4. Compile CEGUI and its dependencies
5. Compile libSSH2 (using the zlib from CEGUI's dependencies)
6. Generate a script named `setenv.bat`, which can be invoked any time in the future to set all the necessary environment variables to build SIGViewer. After running this script, you can open the project from the commandline via `devenv SIGViewer_2010.sln`.

You will need to build X3D and SIGService yourself, using the including solution files. Make sure to use the same version of Visual Studio for this as for everything else!

It should only be necessary to run `setup_env.ps1` once, but if you do run it again it will attempt to detect any existing code and prompt you for what you want to keep and what you want to replace (in this way you can update just one library without having to re-download and rebuild everything).

## Building
If you ran the `setup_env.ps1` script above, then the project should be built by:

1. Launching a new command prompt and running `setenv.bat` (located in the `scripts` directory)
2. Invoking `devenv SIGViewer_2010.sln` to launch Visual Stuio

This will ensure Visual Studio is launched with the correct environment for building against all of the dependencies above, and if you have multiple versions of VS installed it will only launch the one used to build all the dependencies. If you didn't use the setup script, the environment variables you need to set are:

    ### Used for Include Directories:
    ```
    BUILD_SIGSERVICE_INC -- Path to SIGService\Windows\SIGService
    BUILD_X3D_INC        -- Path to X3D\parser\cpp\X3DParser
    JDK_PATH             -- Path to your JDK installation directory
    BUILD_BOOST_INC      -- Path to Boost root
    BUILD_OGRE_INC       -- Path to OgreSDK\include
    BUILD_CEGUI_INC      -- Path to Cegui-src\cegui\include
    BUILD_LIBSSH2_INC    -- Path to libSSH2\include
    BUILD_LIBOVR_INC     -- Path to OculusSDK\LibOVR\Include
    ```

    ### Used for Lib Directories
    ```
    SIGSERVICE_ROOT_PATH -- Path to SIGService
    BUILD_X3D_INC        -- Path to X3D
    JDK_PATH             -- Path to your JDK installation directory
    BOOS_ROOT            -- Path to Boost root
    OGRE_SDK             -- Path to OgreSDK
    LIBSSH2_ROOT_PATH    -- Path to libSSH2 root
    LIBOVR_ROOT_PATH     -- Path to OculusSDK\LibOVR
    ```
