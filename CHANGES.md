# noirb/SIGViewer

This document outlines the differences between this fork of SIGViewer and the [original SIGViewer](https://github.com/SIGVerse/SIGViewer).

## Overview

Briefly, the following changes have been made:

* Support for Oculus Rift CV1 (up to SDK v. 1.6.0)
* Streamlined setup and build process; support for multiple versions of Visual Studio
* Updated to more recent and better-supported version of some libraries

## Details

### Project Setup and Build Process

#### Project Setup

Due to the large number of dependencies the project has, and the need to ensure that many of them are compiled together with the same settings, a setup script, `setup_env.ps1`, to automatically download and configure as many dependencies as possible was written.

Relevant Commits:

[e6c0f21f21bbd38828ae2d1a66f89430fce6812c](https://github.com/noirb/SIGViewer/commit/e6c0f21f21bbd38828ae2d1a66f89430fce6812c)
[ae23891e7cde9a9841397ac5de17ced8533da903](https://github.com/noirb/SIGViewer/commit/ae23891e7cde9a9841397ac5de17ced8533da903)
[f20731adfae56888e6bd2a0784b56613e2a50c17](https://github.com/noirb/SIGViewer/commit/f20731adfae56888e6bd2a0784b56613e2a50c17)
[5c1cca4dcb3103010591f7e45022cc5cec754bad](https://github.com/noirb/SIGViewer/commit/5c1cca4dcb3103010591f7e45022cc5cec754bad)
[d00362eeebeafb6a169bc027e5fd4328d6ca7801](https://github.com/noirb/SIGViewer/commit/d00362eeebeafb6a169bc027e5fd4328d6ca7801)
[62bb212daef592921c666308859e6ce61adac9ae](https://github.com/noirb/SIGViewer/commit/62bb212daef592921c666308859e6ce61adac9ae)

#### Building

To make the build process a bit more flexible and to make debugging build issues easier, the project was configured to reference the necessary headers and libraries via environment variables (which are automatically set by the script mentioned above) rather than with hard-coded paths. This allows users to customize their development environment more easily.

Similar changes were also made in forks of [SIGService](https://github.com/noirb/SIGService) and [X3D](https://github.com/noirb/x3d), so all the relevant SIGVerse projects can be set up and built from the same console.

Relevant Commits:

[87e230ec738a142b762921a7eff2470b14ee8acd](https://github.com/noirb/SIGViewer/commit/87e230ec738a142b762921a7eff2470b14ee8acd)
[32fbb54b4478d8b369cb0856685ba6646eab8e37](https://github.com/noirb/SIGViewer/commit/32fbb54b4478d8b369cb0856685ba6646eab8e37)

### CEGUI 0.8+

The project used to build against a very old version of CEGUI, which is no longer supported and required a great deal of effort to compile. Starting with 0.8 (the currently-supported version), the CEGUI team moved to CMake for building their project, which is much more convenient. This did, however, require making several changes due to many objects having different names in 0.8 and the general format of CEGUI's layout files changing.

Relevant Commits:

[1d49867d330861f59d0db5467968dfa086436579](https://github.com/noirb/SIGViewer/commit/1d49867d330861f59d0db5467968dfa086436579)
[599255e686edbaf59c3e08e6287dde02fe54d0cf](https://github.com/noirb/SIGViewer/commit/599255e686edbaf59c3e08e6287dde02fe54d0cf)
[143626840266b5eeb60263b869d2fa03d1346739](https://github.com/noirb/SIGViewer/commit/143626840266b5eeb60263b869d2fa03d1346739)

### Oculus CV1 Support

The last version of the Oculus SDK to be supported was 0.4.3, which is unfortunately incompatible with the current hardware (even using the DK1, the 0.4.3 SDK is problematic for both Windows 10 and Visual Studio 2015). Updating to the new SDK required rewriting most of the OgreOculus component. Previously, the Oculus Runtime would provide a distortion mesh and other parameters to the application, which could then be used to render a scene like normal, with the Rift acting pretty much just like another monitor from the application's perspective. Starting with SDK 0.8 they moved to a session-oriented model, in which the application no longer talks directly to the HMD at all. Instead, the application must obtain a texture from the runtime, render to that, then notify the runtime a new frame is available. Everything else, from the distortion to the actual display of the image on the HMD screen, is handled by the runtime.

Now, a large Ogre texture is generated at startup to hold images for both eyes, and two cameras are created to render to each half of this texture. Every frame the Ogre texture is updated, then copied into a texture provided by the Oculus runtime and submitted for rendering on the HMD.

Relevant Commits:

[7aefd4e916cf34629e59ade864b29fb68f1e8abc](https://github.com/noirb/SIGViewer/commit/7aefd4e916cf34629e59ade864b29fb68f1e8abc)
[d8bfe66bb96ce8830ca62250c84679672c5fac10](https://github.com/noirb/SIGViewer/commit/d8bfe66bb96ce8830ca62250c84679672c5fac10)
[bb7e9f840f4828d9db1f1df56e8c69b054690a6d](https://github.com/noirb/SIGViewer/commit/bb7e9f840f4828d9db1f1df56e8c69b054690a6d)
[e6b71b33d2d1531523e29fe4621e7bc0c32833db](https://github.com/noirb/SIGViewer/commit/e6b71b33d2d1531523e29fe4621e7bc0c32833db)
[a327d53e9097e8dcdece918a8ed166d9bcdbb288](https://github.com/noirb/SIGViewer/commit/a327d53e9097e8dcdece918a8ed166d9bcdbb288)