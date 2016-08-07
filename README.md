SIGVerse/SIGViewer
=============

How to change normal SIGViewer to oculus SIGViewer.

open: SIGViewer\Release\SIGVerse.ini

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

* [SIGService](https://github.com/noirb/SIGService/tree/dev)
* [X3D Parser](https://github.com/noirb/x3d/tree/dev)
* [The Ogre SDK](http://www.ogre3d.org/download/sdk), version 1.9
* [GL3W](https://github.com/skaslev/gl3w)
* [CEGUI](http://cegui.org.uk/), version 0.8+ (built with Ogre support)
* [Java JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html)
* [Boost](http://www.boost.org/users/history/version_1_61_0.html), version 1.61.0+
* [LibSSH2](https://www.libssh2.org/)
* [OpenSSL](https://www.openssl.org/)
* [Zlib](http://www.zlib.net/) (actually included with CEGUI's dependencies, so not necessary to install separately)
* [Oculus SDK](https://developer.oculus.com/downloads/), version 1.6.0
* [CMake](https://cmake.org/download/)
* (recommended) [7-Zip](http://www.7-zip.org/)

A PowerShell script, `setup_env.ps1`, is provided to help build and configure these in order to get you a working development environment as quickly as possible.
Run this script from the directory you want to make your "SIGVerse Root" (e.g. one directory level above the location you cloned SIGViewer to).
It will:

1. Detect any Visual Studio installations on your machine (and prompt you to choose one if there are several)
2. Check for CMake and 7-Zip, and if they can't be found prompt you for their location (in case you have them on your desktop or something)
3. Download recent versions of all of the above dependencies, excluding [JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html), [CMake](https://cmake.org/download/) and [7-Zip](http://www.7-zip.org/) (which you should install yourself before starting). If you're using a version of Visual Studio newer than 2012, you will also need to [download the Ogre SDK yourself from here](http://ogre3d.org/forums/viewtopic.php?t=69274), since those builds aren't hosted in a location the script can automatically pull from.
4. Unzip them into your "SIGVerse Root" with the following structure:
    `SIGVerseRoot\*        -- SIGVerse projects (SIGViewer, etc.)`
    `SIGVerseRoot\external -- All other dependencies`
5. Compile necessary Boost libraries used by Ogre and CEGUI
6. Compile CEGUI and its dependencies
7. Compile libSSH2 (using the zlib from CEGUI's dependencies)
8. Compile X3D and SIGService
9. Generate a script named `setenv.bat`, which can be invoked any time in the future to set all the necessary environment variables to build SIGViewer. After running this script, you can open the project from the commandline via `devenv SIGViewer_2010.sln`.

Before running the powershell script, you may need to change powershell's execution policy, which is `restricted` by default. To do so, run powershell as an administrator, and then execute the command: `Set-ExecutionPolicy RemoteSigned`

This will allow you to run local scripts, even when *not* running powershell as an administrator (which is not required for any of the scripts in this project, and is in general not recommended unless you really *need* it).

It should only be necessary to run `setup_env.ps1` once, but if you do run it again it will attempt to detect any existing code and prompt you for what you want to keep and what you want to replace (in this way you can update just one library without having to re-download and rebuild everything).

## Building
**Always make sure to use the same compiler and release settings across dependencies where applicable! This means if you're building the 32-bit Release version of SIGViewer, you should be building the 32-bit Release version of all its dependencies as well!**

**You will need to obtain or build [gl3w](https://github.com/skaslev/gl3w) yourself and place it in `<sigverse_root>\extern`**

The `setup_env.ps1` script will generate 32-bit Release builds of all dependecies (if there's demand, it will be updated to allow Debug builds as well, but SIGViewer does not currently support 64-bit builds). To use it, simply run the script from the location you want all of your SIGVerse-related files to live. This is most likely the directory in which you put the SIGViewer code (e.g. if SIGViewer is in `C:\SIGVerse\SIGViewer`, then `C:\SIGVerse` is the `SIGVerse Root` you want to run the script from).

The script will first check to ensure Visual Studio, CMake, and 7-zip are installed, then allow you to confirm that the correct versions of each are selected before beginning. It will then download and set up the necessary dependecies, and give you one last chance to confirm or cancel before building them all. If any errors are detected they should be logged in the script output.

```
    C:\Users\nao\SIGVerse> .\SIGViewer\setup_env.ps1 
    Found the following Visual Studio installations:
    [ 1 ] : Visual Studio 14 2015
    Selected: Visual Studio 14 2015
    --
    Please download the OGRE SDK for your compiler from here: http://ogre3d.org/forums/viewtopic.php?t=69274
    And extract it into the directory: C:\Users\nao\SIGVerse\extern\OGRE-SDK-1.9.0-vc140-x86-12.03.2016
    --
    Press enter to continue...:

    7-Zip not found in PATH. Searching for installation directory.

    Current working directory: C:\Users\nao\SIGVerse
    =========================================================
        Temporary files will be copied to:   C:\Users\nao\SIGVerse\setup_tmp
        SIGVerse projects will be set up in: C:\Users\nao\SIGVerse
        Dependencies will be set up in:      C:\Users\nao\SIGVerse\extern
        Visual Studio:
                      Version: Visual Studio 14 2015
                      Tools:   C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\
        7-Zip Directory:                     C:\Program Files\7-Zip
        CMake Directory:                     C:\Program Files\CMake\bin
    =========================================================
    Proceed with this configuration? (y/n): y
```

If you ran the `setup_env.ps1` script above and it completed without errors, then you're ready to build the SIGViewer project!

1. Launch a new command prompt and run `setenv.bat` (located in the `scripts` directory of the SIGViewer project)
2. Invoke `devenv SIGViewer_2010.sln` to launch Visual Studio with all the correct environment variables set

This will ensure Visual Studio is launched with the correct environment for building against all of the dependencies above, and if you have multiple versions of VS installed it will only launch the one used to build all the dependencies so there's no chance of mixing up compiler versions. There are forks of [X3D](https://github.com/noirb/x3d/tree/dev) and [SIGService](https://github.com/noirb/SIGService/tree/dev) available which use the same environment variables for their builds, so everything can be built from the same command prompt.

If you didn't use the setup script and have built or obtained the dependencies above in another way, the environment variables you need to set are:

### Used for Include Directories:

    BUILD_SIGSERVICE_INC -- Path to SIGService\Windows\SIGService
    BUILD_X3D_INC        -- Path to X3D\parser\cpp\X3DParser
    JDK_ROOT_PATH        -- Path to your JDK installation directory
    BUILD_BOOST_INC      -- Path to Boost root
    BUILD_OGRE_INC       -- Path to OgreSDK\include
    BUILD_CEGUI_INC      -- Path to Cegui-src\cegui\include
    BUILD_LIBSSH2_INC    -- Path to libSSH2\include
    BUILD_LIBOVR_INC     -- Path to OculusSDK\LibOVR\Include


### Used for Lib Directories

    BUILD_SIGSERVICE_LIB -- Path to SIGService\Windows\<Target>\
    BUILD_X3D_LIB        -- Path to X3D\parser\cpp\<Target>\
    JDK_ROOT_PATH        -- Path to your JDK installation directory
    BUILD_BOOST_LIB      -- Path to Boost\<staging_dir>\lib
    BUILD_OGRE_LIB       -- Path to OgreSDK\lib
    BUILD_LIBSSH2_LIB    -- Path to libSSH2\<build_dir>\src\<Target>\
    BUILD_LIBOVR_LIB     -- Path to OculusSDK\LibOVR\Lib\Windows\Win32\Release\<VS_Version>\


Either set these globally or through a batch script before launching Visual Studio and everything should be fine.

## Running SIGViewer

Still with me? :)

One final step which must be taken before SIGViewer will run is to ensure the executable has access to all the resources it depends on. The first step is to ensure that all the DLLs it links against are in a location it can find them. The easiest (though perhaps not the cleanest) way to do this is to copy them directly into the build folder next to SIGViewer.exe. You will need to copy:

* The System, Thread, and Chrono DLLs from `BOOST_ROOT\stage\lib` (or whatever other staging directory you specified if you built Boost yourself)
* All the DLLs which do not end in "Demo" from `CEGUI_ROOT\bin`
* All the DLLs from `CEGUI_ROOT\dependencies\bin`
* All the DLLs from `OGRE_SDK\bin\Release` which *do not* start with "Sample_"
* `libeay32.dll` and `ssleay32.dll` from the OpenSSL bin directory
* And last but not least, SIGViewer and Ogre both require a small collection of configuration and resource files. Since these are spread out across multple projects, the easiest thing to do to get the viewer running is to [download this package](https://mega.nz/#!95ZkGDYJ!4mEQfQfCoCpJIjTYvYq-jntqqfubbIsZHPR5WeTEPqs) and unzip it into your binary directory. (there should be a script to collect these all for you, but this will have to make do for now).

## Notes
Although everything **should** work as long as you're using VS 2010 or newer, not every combination of compiler and library version has been tested, so it's possible you might run into issues depending on your configuration.

The most recently-tested versions of all dependecies found to work together were:

#### With Visual Studio 2015
* [Ogre SDK](http://ogre3d.org/forums/viewtopic.php?t=69274) 1.9.0-vc140-x86-12.03.2016
* [CEGUI](http://prdownloads.sourceforge.net/crayzedsgui/cegui-0.8.7.zip) 0.8.7, and [CEGUI Deps](http://prdownloads.sourceforge.net/crayzedsgui/cegui-deps-0.8.x-src.zip) 0.8.x
* [Java JDK](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html) 1.8.0_101
* [Boost](http://downloads.sourceforge.net/project/boost/boost/1.61.0/boost_1_61_0.zip) 1.61.0
* [libssh2](https://www.libssh2.org/download/libssh2-1.7.0.tar.gz) 1.7.0
* [openssl](https://openssl-for-windows.googlecode.com/files/openssl-0.9.8k_WIN32.zip) 0.9.8k
* [Oculus SDK](https://static.oculus.com/sdk-downloads/1.6.0/Public/1468518301/ovr_sdk_win_1.6.0_public.zip) 1.6.0
* [CMake](https://cmake.org/download/) 3.6.1
    
#### With Visual Studio 2012
* [Ogre SDK](http://downloads.sourceforge.net/project/ogre/ogre/1.9/1.9/OgreSDK_vc11_v1-9-0.exe) 1.9.0-vc11-x86
* [CEGUI](http://prdownloads.sourceforge.net/crayzedsgui/cegui-0.8.7.zip) 0.8.7, and [CEGUI Deps](http://prdownloads.sourceforge.net/crayzedsgui/cegui-deps-0.8.x-src.zip) 0.8.x
* [Java JDK](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html) 1.8.0_101
* [Boost](http://www.boost.org/users/history/version_1_55_0.html) 1.55.0
* [libssh2](https://www.libssh2.org/download/libssh2-1.7.0.tar.gz) 1.7.0
* [openssl](https://openssl-for-windows.googlecode.com/files/openssl-0.9.8k_WIN32.zip) 0.9.8k
* [Oculus SDK](https://static.oculus.com/sdk-downloads/1.6.0/Public/1468518301/ovr_sdk_win_1.6.0_public.zip) 1.6.0
* [CMake](https://cmake.org/download/) 3.6.1
