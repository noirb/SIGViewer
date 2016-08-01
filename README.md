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
* [The Ogre SDK](http://www.ogre3d.org/download/sdk), version 1.9
* [CEGUI](http://cegui.org.uk/), version 0.8+ (built with Ogre support)
* [Java JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html)
* [Boost](http://www.boost.org/users/history/version_1_61_0.html), version 1.61.0+
* [LibSSH2](https://www.libssh2.org/)
* [OpenSSL](https://www.openssl.org/)
* [Zlib](http://www.zlib.net/) (actually included with CEGUI's dependencies, so not necessary to install separately)
* [Oculus SDK](https://developer.oculus.com/downloads/), version 0.8.0
* [CMake](https://cmake.org/download/)
* (recommended) [7-Zip](http://www.7-zip.org/)

A PowerShell script, `setup_env.ps1`, is provided to help build and configure these in order to get you a working development environment as quickly as possible.
Run this script from the directory you want to make your "SIGVerse Root" (e.g. one directory level above the location you cloned SIGViewer to).
It will:

1. Detect any Visual Studio installations on your machine and prompt you to choose one
2. Download recent versions of all of the above dependencies, excluding [JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html), [CMake](https://cmake.org/download/) and [7-Zip](http://www.7-zip.org/) (which you should install yourself before starting). If you're using a version of Visual Studio newer than 2012, you will also need to [download the Ogre SDK yourself from here](http://ogre3d.org/forums/viewtopic.php?t=69274), since those builds aren't hosted in a location the script can automatically pull from.
3. Unzip them into your "SIGVerse Root" with the following structure:
    `SIGVerseRoot\*        -- SIGVerse projects (SIGViewer, etc.)`
    `SIGVerseRoot\external -- All other dependencies`
4. Compile necessary Boost libraries used by Ogre and CEGUI
5. Compile CEGUI and its dependencies
6. Compile libSSH2 (using the zlib from CEGUI's dependencies)
7. Generate a script named `setenv.bat`, which can be invoked any time in the future to set all the necessary environment variables to build SIGViewer. After running this script, you can open the project from the commandline via `devenv SIGViewer_2010.sln`.

You will need to build X3D and SIGService yourself, using the including solution files. Make sure to use the same version of Visual Studio for this as for everything else!

Before running the powershell script, you may need to change powershell's execution policy, which is `restricted` by default. To do so, run powershell as an administrator, and then execute the command: `Set-ExecutionPolicy RemoteSigned`

This will allow you to run local scripts, even when *not* running powershell as an administrator (which is not required for any of the scripts in this project, and is in general not recommended unless you really *need* it).

It should only be necessary to run `setup_env.ps1` once, but if you do run it again it will attempt to detect any existing code and prompt you for what you want to keep and what you want to replace (in this way you can update just one library without having to re-download and rebuild everything).

## Building
**Always make sure to use the same compiler and release settings across dependencies where applicable! This means if you're building the 32-bit Release version of SIGViewer, you should be building the 32-bit Release version of all its dependencies as well!**

If you ran the `setup_env.ps1` script above and it completed without errors, then you just need to build the Win32|Release targets of X3d and SIGService, then the SIGViewer project should be built by:

1. Launching a new command prompt and running `setenv.bat` (located in the `scripts` directory)
2. Invoking `devenv SIGViewer_2010.sln` to launch Visual Stuio

This will ensure Visual Studio is launched with the correct environment for building against all of the dependencies above, and if you have multiple versions of VS installed it will only launch the one used to build all the dependencies. If you didn't use the setup script and have built or obtained the dependencies above, the environment variables you need to set are:

    ### Used for Include Directories:
    ```
    BUILD_SIGSERVICE_INC -- Path to SIGService\Windows\SIGService
    BUILD_X3D_INC        -- Path to X3D\parser\cpp\X3DParser
    JDK_ROOT_PATH        -- Path to your JDK installation directory
    BUILD_BOOST_INC      -- Path to Boost root
    BUILD_OGRE_INC       -- Path to OgreSDK\include
    BUILD_CEGUI_INC      -- Path to Cegui-src\cegui\include
    BUILD_LIBSSH2_INC    -- Path to libSSH2\include
    BUILD_LIBOVR_INC     -- Path to OculusSDK\LibOVR\Include
    ```

    ### Used for Lib Directories
    ```
    BUILD_SIGSERVICE_LIB -- Path to SIGService\Windows\<Target>\
    BUILD_X3D_LIB        -- Path to X3D\parser\cpp\<Target>\
    JDK_ROOT_PATH        -- Path to your JDK installation directory
    BUILD_BOOST_LIB      -- Path to Boost\<staging_dir>\lib
    BUILD_OGRE_LIB       -- Path to OgreSDK\lib
    BUILD_LIBSSH2_LIB    -- Path to libSSH2\<build_dir>\src\<Target>\
    BUILD_LIBOVR_LIB     -- Path to OculusSDK\LibOVR\Lib\Windows\Win32\Release\<VS_Version>\
    ```
