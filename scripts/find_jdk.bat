@echo off
rem adapted from: http://stackoverflow.com/a/25249886


setlocal

::- Get the JDK Version
set KEY="HKLM\SOFTWARE\JavaSoft\Java Development Kit"
set VALUE=CurrentVersion
reg query %KEY% /v %VALUE% >nul 2>nul || (
    echo ERROR: JDK not installed 
    exit /b 1
)

set JDK_VERSION=
for /f "tokens=2,*" %%a in ('reg query %KEY% /v %VALUE% ^| findstr %VALUE%') do (
    set JDK_VERSION=%%b
)

rem echo JDK VERSION: %JDK_VERSION%

::- Get the JavaHome key
set KEY="HKLM\SOFTWARE\JavaSoft\Java Development Kit\%JDK_VERSION%"
set VALUE=JavaHome
reg query %KEY% /v %VALUE% >nul 2>nul || (
    echo ERROR: JavaHome not found :(
    exit /b 1
)

set JDK_ROOT=
for /f "tokens=2,*" %%a in ('reg query %KEY% /v %VALUE% ^| findstr %VALUE%') do (
    set JDK_ROOT=%%b
)

echo JDK Root: %JDK_ROOT%

endlocal & set JDK_ROOT_PATH=%JDK_ROOT%