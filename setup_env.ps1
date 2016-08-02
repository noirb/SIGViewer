
$scriptPath = split-path -parent $MyInvocation.MyCommand.Definition
# The root directory of the whole project
$projectRoot = $PWD.Path

# A directory to store temporary downloaded files
$tmp_dir = "$projectRoot\setup_tmp"
if ( !(Test-Path $tmp_dir) ) {
    new-item $tmp_dir -type directory
}

# The root directory to store non-SIGVerse dependencies in
$projectDepsRoot = "$projectRoot\extern"
if ( !(Test-Path $projectDepsRoot) ) {
    new-item $projectDepsRoot -type directory
}

# environment variables used to build the project
$build_vars = @{
    "OGRE_SDK"             = "";
    "CEGUI_ROOT_PATH"      = "";
    "CEGUI_DEPS_ROOT"      = "";
    "LIBSSH2_ROOT_PATH"    = "";
    "OPENSSL_ROOT_DIR"     = "";
    "X3D_ROOT_PATH"        = "";
    "SIGSERVICE_ROOT_PATH" = "";
    "BOOST_ROOT"           = "";
    "LIBOVR_ROOT_PATH"     = "";
    "VS_TOOLS_PATH"        = "";
}

# These are used as an argument to CMake when generating project files
$vsVersionNames = @{
    "VS14" = "Visual Studio 14 2015";
    "VS12" = "Visual Studio 12 2013";
    "VS11" = "Visual Studio 11 2012";
    "VS10" = "Visual Studio 10 2010";
    "VS09" = "Visual Studio 9 2008"
}

# Find installed visual studio versions
$vsEnvVars = @()

foreach ($item in (dir Env:)) {
	if ($item.name -Match "VS[0-9]{1,3}COMNTOOLS") {
		$vsEnvVars = $vsEnvVars + $item
	}
}

# If no VS installation was found, bail
if ($vsEnvVars.length -eq 0) {
	echo 'No Version of Visual Studio was detected! Please ensure you have VS 2010 or later installed!'
	echo 'Exiting...'
	exit
}

echo 'Found the following Visual Studio installations:'
$iter = 1
foreach ($item in $vsEnvVars) {
    $shortName = $item.Key.Substring(0,4)
    if ($vsVersionNames.ContainsKey($shortName))
    {
        write-host [ $iter ] : $vsVersionNames[$shortName]
    }
    else
    {
        write-host [ $iter ] : Unknown VS Version: $iter.Key
    }
    $iter++
}

$vsVersion = ""
$vsToolsPath = ""

if ($vsEnvVars.length -gt 1) {

    do{
        $resp = Read-Host -prompt "Select Version (1-$($iter-1))"
        [int]$choice = $null
        [void][int32]::TryParse( $resp, [ref]$choice )
        if ( !$choice -Or $choice -lt 1 -Or $choice -ge $iter)
        {
          write-host "Please enter a number between 1 and $($iter-1)"
        }
        else
        {
            $vsVersion   = $vsVersionnames[$vsEnvVars[$choice-1].Key.Substring(0,4)]
            $vsToolsPath = $vsEnvVars[$choice-1].Value
            write-host "Selected: $vsVersion"
        }
      } until ($vsVersion.length -gt 0 -And $vsToolsPath.length -gt 0)
}
else {
    $vsVersion   = $vsVersionNames[$vsEnvVars[0].Key.Substring(0,4)]
    $vsToolsPath = $vsEnvVars[0].Value
    write-host "Selected: $vsVersion"
}

# if we're using VS 2015 or 2013, OGRE SDK must be manually downloaded...
if ($vsVersion -eq "Visual Studio 14 2015")
{
    write-host "--"
    write-host "Please download the OGRE SDK for your compiler from here: http://ogre3d.org/forums/viewtopic.php?t=69274"
    write-host "And extract it into the directory: $projectDepsRoot\OGRE-SDK-1.9.0-vc140-x86-12.03.2016"
    write-host "--"
    Read-Host "Press enter to continue..."
    $build_vars.Set_Item("OGRE_SDK", "$projectDepsRoot\OGRE-SDK-1.9.0-vc140-x86-12.03.2016")
}
if ($vsVersion -eq "Visual Studio 12 2013")
{
    write-host "--"
    write-host "Please download the OGRE SDK for your compiler from here: http://ogre3d.org/forums/viewtopic.php?t=69274"
    write-host "And extract it into the directory: $projectDepsRoot\OGRE-SDK-1.9.0-vc120-x86-12.03.2016"
    write-host "--"
    Read-Host "Press enter to continue..."
    $build_vars.Set_Item("OGRE_SDK", "$projectDepsRoot\OGRE-SDK-1.9.0-vc120-x86-12.03.2016")
}

# Find 7-Zip
$7zPath = ''
foreach ($path in (($Env:path).split(";"))) {
    if ($path -like "*7-Zip*" ) {
        $7zPath = $path
    }
}
# if it wasn't in the PATH, go hunting in program files...
if (!$7zPath) {
    echo "7-Zip not found in PATH. Searching for installation directory."
    if (Test-Path "$Env:ProgramFiles\7-Zip") {
        $7zPath = "$Env:ProgramFiles\7-Zip"
    }
    elseif (Test-Path "${Env:ProgramFiles(x86)}\7-Zip") {
        $7zPath = "${Env:ProgramFiles(x86)}\7-Zip"
    }
    # if it wasn't found, ask the user for a path in case they have a portable copy
    else {
        echo "ERROR: Could not find 7-Zip!"
        echo "Please enter the path to your 7-Zip directory (ctrl+C to cancel)"
        $7zPath = Read-Host -prompt "7-Zip Path"
    }
}
$7zX = "& ""$7zPath\7z.exe"" x -y"

# Find CMake
$cmakePath = ''
foreach ($path in (($Env:path).split(";"))) {
    if ($path -like "*CMake*" ) {
        $cmakePath = $path
    }
}
if (!$cmakePath) {
    echo "CMake not found in PATH. Searching for installation directory."
    foreach ($path in (dir ${Env:ProgramFiles(x86)})) {
        if ($path -like "*CMake*") {
            $cmakePath = "${Env:ProgramFiles(x86)}\$path\bin"
        }
    }
    foreach ($path in (dir $Env:ProgramFiles)) {
        if ($path -like "*CMake*") {
            $cmakePath = "$($Env:ProgramFiles)\$path\bin"
        }
    }
}
if (!$cmakePath) {
    echo "ERROR: Could not find CMake!"
    echo "Please enter the path to your CMake installation directory (ctrl+C to cancel)"
    $cmakePath = Read-Host -prompt "CMake Path"
}
$cmake = """$cmakePath\cmake.exe"""


$build_vars.Set_Item("VS_TOOLS_PATH", $vsToolsPath)

echo ""
echo "Current working directory: $projectRoot"
echo "========================================================="
echo "    Temporary files will be copied to:   $tmp_dir"
echo "    SIGVerse projects will be set up in: $projectRoot"
echo "    Dependencies will be set up in:      $projectDepsRoot"
echo "    Visual Studio:"
echo "                  Version: $vsVersion"
echo "                  Tools:   $($build_vars.Get_Item(""VS_TOOLS_PATH""))"
echo "    7-Zip Directory:                     $7zPath"
echo "    CMake Directory:                     $cmakePath"
echo "========================================================="

if ( !((Read-Host -Prompt "Proceed with this configuration? (y/n)").ToLower().StartsWith("y")) ) {
    exit
}

# Locations to download dependencies from
## TODO: check for git and prefer cloning over downloading when possible
$SIGViewer_www  = "https://github.com/noirb/SIGViewer/archive/dev.zip"
$SIGService_www = "https://github.com/noirb/SIGService/archive/dev.zip"
$X3D_www        = "https://github.com/noirb/x3d/archive/dev.zip"
$CEGUI_www      = "http://prdownloads.sourceforge.net/crayzedsgui/cegui-0.8.7.zip"
$CEGUI_deps_www = "http://prdownloads.sourceforge.net/crayzedsgui/cegui-deps-0.8.x-src.zip"
$libSSH2_www    = "https://www.libssh2.org/download/libssh2-1.7.0.tar.gz"
$openSSL_www    = "https://openssl-for-windows.googlecode.com/files/openssl-0.9.8k_WIN32.zip"
$boost_www      = "http://downloads.sourceforge.net/project/boost/boost/1.61.0/boost_1_61_0.zip"
$libovr_www     = "https://static.oculus.com/sdk-downloads/0.8.0.0/Public/1445451746/ovr_sdk_win_0.8.0.0.zip"

# set download URL for Ogre only if we're using VS 2010 or 2012
if ($vsVersion -eq "Visual Studio 11 2012")
{
	$Ogre_SDK_www   = "http://downloads.sourceforge.net/project/ogre/ogre/1.9/1.9/OgreSDK_vc11_v1-9-0.exe"
}
if ($vsVersion -eq "Visual Studio 10 2010")
{
	$Ogre_SDK_www   = "http://downloads.sourceforge.net/project/ogre/ogre/1.9/1.9/OgreSDK_vc10_v1-9-0.exe"
}

$net = new-object System.Net.WebClient

function doDownload
{
    param( [string]$url, [string]$destDir, [string]$destFile, [System.Net.WebClient]$wc )
    
    if ( Test-Path "$destDir\$destFile" ) {
        $conf = Read-Host -Prompt "An existing $destFile was found! Download a fresh copy? (y/n)"
        if ( $conf.ToLower().StartsWith("y") ) {
            echo "Downloading $destFile..."
            $wc.DownloadFile($url, "$destDir\$destFile");
        }
    }
    else {
        echo "Downloading $destFile..."
        $wc.DownloadFile($url, "$destDir\$destFile");
    }
}

# download all 
doDownload -url $SIGViewer_www  -destFile "sigviewer.zip"  -destDir $tmp_dir -wc $net
doDownload -url $SIGService_www -destFile "sigservice.zip" -destDir $tmp_dir -wc $net
doDownload -url $X3D_www        -destFile "x3d.zip"        -destDir $tmp_dir -wc $net
doDownload -url $CEGUI_www      -destFile $CEGUI_www.Substring($CEGUI_www.LastIndexOf("/") + 1)           -destDir $tmp_dir -wc $net
doDownload -url $CEGUI_deps_www -destFile $CEGUI_deps_www.Substring($CEGUI_deps_www.LastIndexOf("/") + 1) -destDir $tmp_dir -wc $net
doDownload -url $libSSH2_www    -destFile $libSSH2_www.Substring($libSSH2_www.LastIndexOf("/") + 1)       -destDir $tmp_dir -wc $net
doDownload -url $openSSL_www    -destFile $openSSL_www.Substring($openSSL_www.LastIndexOf("/") + 1)       -destDir $tmp_dir -wc $net
doDownload -url $boost_www      -destFile $boost_www.Substring($boost_www.LastIndexOf("/") + 1)           -destDir $tmp_dir -wc $net
doDownload -url $libovr_www     -destFile $libovr_www.Substring($libovr_www.LastIndexOf("/") + 1)         -destDir $tmp_dir -wc $net

# only download Ogre if we're using VS 2012 or earlier
if ($vsVersion -eq "Visual Studio 10 2010" -Or $vsVersion -eq "Visual Studio 11 2012")
{
	doDownload -url $Ogre_SDK_www   -destFile $Ogre_SDK_www.Substring($Ogre_SDK_www.LastIndexOf("/") + 1)     -destDir $tmp_dir -wc $net
}

# check for existing code
foreach ($item in (dir $projectRoot)) {
    if ($item.Name.ToLower() -like "*sigviewer*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the SIGViewer source directory? (y/n)"
        if (!($conf.ToLower().StartsWith("y"))) {
            remove-item "$projectRoot\$($item.Name)" -recurse
        }
    }
    elseif ($item.Name.ToLower() -like "*sigservice*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the SIGService source directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("SIGSERVICE_ROOT_PATH", "$projectRoot\$($item.Name)")
        } else {
            remove-item "$projectRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*x3d*") {
        echo "Existing directory found: $($item.Name)"
        $conf = read-Host -prompt "Should this be used as-is as the X3D source directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("X3D_ROOT_PATH", "$projectRoot\$($item.Name)")
        } else {
            remove-item "$projectRoot\$($item.Name)"
        }
    }
}
foreach ($item in (dir $projectDepsRoot)) {
    if ($item.Name.ToLower() -like "*ogre*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the Ogre SDK directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("OGRE_SDK", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*cegui-[0-9]*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the CEGUI source directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("CEGUI_ROOT_PATH", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*cegui-deps*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the CEGUI DEPS directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("CEGUI_DEPS_ROOT", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*libssh2*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the libSSH2 directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("LIBSSH2_ROOT_PATH", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*openssl*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the OpenSSL directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("OPENSSL_ROOT_DIR", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*boost*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the Boost source directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("BOOST_ROOT", "$projectDepsRoot\$($item.Name)")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
    elseif ($item.Name.ToLower() -like "*oculussdk*") {
        echo "Existing directory found: $($item.Name)"
        $conf = Read-Host -prompt "Should this be used as-is as the Oculus SDK directory? (y/n)"
        if (($conf.ToLower().StartsWith("y"))) {
            $build_vars.Set_Item("LIBOVR_ROOT_PATH", "$projectDepsRoot\$($item.Name)\LibOVR")
        } else {
            remove-item "$projectDepsRoot\$($item.Name)"
        }
    }
}


# extract archives for any code not found above
if (!($build_vars.Get_Item("SIGSERVICE_ROOT_PATH"))) {
    iex "$7zX -o$projectRoot $tmp_dir\sigservice.zip"
    $build_vars.Set_Item("SIGSERVICE_ROOT_PATH", "$projectRoot\SIGService-master")
}

if (!($build_vars.Get_Item("X3D_ROOT_PATH"))) {
    iex "$7zX -o$projectRoot $tmp_dir\x3d.zip"
    $build_vars.Set_Item("X3D_ROOT_PATH", "$projectRoot\x3d-master")
}

if (!($build_vars.Get_Item("OGRE_SDK"))) {
    iex "$tmp_dir\$($Ogre_SDK_www.Substring($Ogre_SDK_www.LastIndexOf(""/"") + 1)) -o$projectDepsRoot -y"
    $build_vars.Set_Item("OGRE_SDK", "$projectDepsRoot\OgreSDK_vc11_v1-9-0")
}

if (!($build_vars.Get_Item("CEGUI_ROOT_PATH"))) {
    iex "$7zX -o$projectDepsRoot $tmp_dir\$($CEGUI_www.Substring($CEGUI_www.LastIndexOf(""/"") + 1))"
    $build_vars.Set_Item("CEGUI_ROOT_PATH", "$projectDepsRoot\cegui-0.8.7")
}

if (!($build_vars.Get_Item("CEGUI_DEPS_ROOT"))) {
    iex "$7zX -o$projectDepsRoot $tmp_dir\$($CEGUI_deps_www.Substring($CEGUI_deps_www.LastIndexOf(""/"") + 1))"
    $build_vars.Set_Item("CEGUI_DEPS_ROOT", "$projectDepsRoot\cegui-deps-0.8.x-src")
}

if (!($build_vars.Get_Item("LIBSSH2_ROOT_PATH"))) {
    iex "$7zX -o$tmp_dir $tmp_dir\$($libSSH2_www.Substring($libSSH2_www.LastIndexOf(""/"") + 1))"
    iex "$7zX -o$projectDepsRoot $tmp_dir\libssh2-1.7.0.tar"
    $build_vars.Set_Item("LIBSSH2_ROOT_PATH", "$projectDepsRoot\libssh2-1.7.0")
}

if (!($build_vars.Get_Item("OPENSSL_ROOT_DIR"))) {
    $filename = "$($openSSL_www.Substring($openSSL_www.LastIndexOf(""/"") + 1))"
    iex "$7zX -o$projectDepsRoot\$($filename.substring(0, $filename.length-4)) $tmp_dir\$filename"
    $build_vars.Set_Item("OPENSSL_ROOT_DIR", "$projectDepsRoot\openssl-0.9.8k_WIN32")
}

if (!($build_vars.Get_Item("BOOST_ROOT"))) {
    iex "$7zX -o$projectDepsRoot $tmp_dir\$($boost_www.Substring($boost_www.LastIndexOf(""/"") + 1))"
    $build_vars.Set_Item("BOOST_ROOT", "$projectDepsRoot\boost_1_61_0")
}

if (!($build_vars.Get_Item("LIBOVR_ROOT_PATH"))) {
    iex "$7zX -o$projectDepsRoot $tmp_dir\$($libovr_www.Substring($libovr_www.LastIndexOf(""/"") + 1))"
    $build_vars.Set_Item("LIBOVR_ROOT_PATH", "$projectDepsRoot\OculusSDK\LibOVR")
}


echo ''
echo '=============================='
echo '     BUILD CONFIGURATION      '
echo '=============================='
echo "    Project Root:`t   $projectRoot"
echo "    CMake:       `t   $cmakePath"
foreach ($item in $build_vars.GetEnumerator()) {
    "{0, -3} {1, -22} {2, -80}" -f `
    "", $($item.Name + ":"), $item.Value
}
echo "    Visual Studio:"
echo "                  Version: $vsVersion"
echo "                  Tools:   $($build_vars.Get_Item(""VS_TOOLS_PATH""))"
echo ''
$t = Read-Host "Press Enter to continue..."

$dev_script = "$scriptPath\scripts\setenv.bat"

sc $dev_script '' -en ASCII
ac $dev_script '@echo off'
ac $dev_script 'title S I G V E R S E  x86 Release'
ac $dev_script 'rem --------------------------------------------------------'
ac $dev_script 'rem This is a script-generated file! Edit at your own risk!'
ac $dev_script 'rem --------------------------------------------------------'
ac $dev_script "set SIGVERSE_ROOT=""$projectRoot"""
ac $dev_script 'echo Checking for JDK path...'
ac $dev_script 'call .\find_jdk.bat'

ac $dev_script "set CMAKE=$cmake"
ac $dev_script "set VS_VERSION=""$vsVersion"""

foreach ($item in $build_vars.GetEnumerator()) {
    ac $dev_script "set $($item.Name)=""$($item.Value)"""
    ac $dev_script "if not exist %$($item.Name)% ("
    ac $dev_script "  echo $($item.Name) directory could not be found: %$($item.Name)%"
    ac $dev_script "  goto error"
    ac $dev_script ")"
}

# Include paths used by VS
ac $dev_script "set BUILD_SIGSERVICE_INC=$($build_vars.Get_Item(""SIGSERVICE_ROOT_PATH""))\Windows\SIGService"
ac $dev_script "set BUILD_X3D_INC=$($build_vars.Get_Item(""X3D_ROOT_PATH""))\parser\cpp\X3DParser"
ac $dev_script "set BUILD_OGRE_INC=$($build_vars.Get_Item(""OGRE_SDK""))\include"
ac $dev_script "set BUILD_BOOST_INC=$($build_vars.Get_Item(""BOOST_ROOT""))"
ac $dev_script "set BUILD_CEGUI_INC=$($build_vars.Get_Item(""CEGUI_ROOT_PATH""))\cegui\include"
ac $dev_script "set BUILD_LIBSSH2_INC=$($build_vars.Get_Item(""LIBSSH2_ROOT_PATH""))\include"
ac $dev_script "set BUILD_OPENSSL_INC=$($build_vars.Get_Item(""OPENSSL_ROOT_DIR""))\include"
ac $dev_script "set BUILD_LIBOVR_INC=$($build_vars.Get_Item(""LIBOVR_ROOT_PATH""))\Include;$($build_vars.Get_Item(""LIBOVR_ROOT_PATH""))\LibOVRKernel\Src"

# Lib paths used by VS
ac $dev_script "set BUILD_SIGSERVICE_LIB=$($build_vars.Get_Item(""SIGSERVICE_ROOT_PATH""))\Windows\Release_2010"
ac $dev_script "set BUILD_X3D_LIB=$($build_vars.Get_Item(""X3D_ROOT_PATH""))\parser\cpp\Release"
ac $dev_script "set BUILD_OGRE_LIB=$($build_vars.Get_Item(""OGRE_SDK""))\lib"
ac $dev_script "set BUILD_BOOST_LIB=$($build_vars.Get_Item(""BOOST_ROOT""))\stage\lib"
ac $dev_script "set BUILD_CEGUI_LIB=$($build_vars.Get_Item(""CEGUI_ROOT_PATH""))\lib;$($build_vars.Get_Item(""CEGUI_ROOT_PATH""))\dependencies\lib\static"
ac $dev_script "set BUILD_LIBSSH2_LIB=$($build_vars.Get_Item(""LIBSSH2_ROOT_PATH""))\build\src\Release"
ac $dev_script "set BUILD_OPENSSL_LIB=$($build_vars.Get_Item(""OPENSSL_ROOT_DIR""))\lib"
ac $dev_script "set BUILD_ZLIB_LIB=$($build_vars.Get_Item(""CEGUI_ROOT_PATH""))\dependencies\lib\static"
ac $dev_script "set BUILD_LIBOVR_LIB=$($build_vars.Get_Item(""LIBOVR_ROOT_PATH""))\Lib\Windows\Win32\Release\VS2015"

ac $dev_script "call %VS_TOOLS_PATH%\VsDevCmd.bat"

#ac $dev_script 'endlocal'
ac $dev_script 'if errorlevel 0 goto end'

ac $dev_script ':error'
ac $dev_script 'exit /B 1'

ac $dev_script ':end'


# Attempt to build all dependencies
cd "$scriptPath\scripts"
cmd /C 'build_boost.bat'
if ( $lastexitcode -ne 0) {
    echo 'ERROR: Boost build failed! Terminating build scripts'
    cd $projectRoot
    exit
}

cmd /C 'build_cegui.bat'
if ( $lastexitcode -ne 0) {
    echo 'ERROR: CEGUI build failed! Terminating build scripts'
    cd $projectRoot
    exit
}

cmd /C 'build_libssh2.bat'
if ( $lastexitcode -ne 0) {
    echo 'ERROR: LIBSSH2 build failed! Terminating build scripts'
    cd $projectRoot
    exit
}

cmd /C 'build_x3d.bat'
if ( $lastexitcode -ne 0) {
    echo 'ERROR: X3D build failed! Terminating build scripts'
    cd $projectRoot
    exit
}

cmd /C 'build_sigservice.bat'
if ( $lastexitcode -ne 0) {
    echo 'ERROR: SIGService build failed! Terminating build scripts'
    cd $projectRoot
    exit
}

echo ""
echo " ======================= "
echo " SIGVerse SETUP COMPLETE"
echo " ======================= "
echo ""
cd $projectRoot
