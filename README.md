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



**********************************************
 Configure on Windows OS for develop members
**********************************************

- Basically, you should use Visual C++ 2010.
- If the old project used VC 2008, the solution files should contain the string vs2008/vs2010 such as SIGService_vs2008.sln, SIGService_vs2010.sln in order to avoid confusion
- SIGVerse project always uses 32bit application. You should not use 64bit application.


The following develop environment should be shared among all the developer members. You should not put source codes at different folder.  
Everything should be put at C:\SIGVerse   
C:\SIGVerse\lib     : Target folder of libraries such as SIGService. *dll is desirable, *lib should be avoided.  
C:\SIGVerse\include : A folder which has header file for building libraries and plugins. Mainly used by users.  
C:\SIGVerse\src     : Source codes which are required to build plugins, but should not be distributed by installer and GitHub.  
Of course, we never put files, kept by the above three folders, to GitHub.

The following folders should be used to build SIGVerse application on Windows.  
C:\SIGVerse\GitHub\SIGViewer\....  
C:\SIGVerse\GitHub\SIGService\....  
C:\SIGVerse\GitHub\Plugin\....  
C:\SIGVerse\ext\boost_1_55_0  
C:\SIGVerse\ext\OculusSDK  
C:\SIGVerse\ext\ogre_src_v1-8....  
C:\SIGVerse\ext\CEGUI-****

