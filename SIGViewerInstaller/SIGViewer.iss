[Setup]
<<<<<<< HEAD
AppId={{072CDBA3-0429-4F16-B6DE-4862CD792F9D}}
=======
AppId={{63A7E5E4-7859-4661-A386-020E854DB851}}
>>>>>>> master
// Application name
AppName=SIGViewer
// Application name and version number                    
AppVerName=SIGViewer 2.3.2
AppPublisher=National Institute of Informatics
AppPublisherURL=http://www.sigverse.org/
AppSupportURL=http://www.sigverse.org/
AppUpdatesURL=http://www.sigverse.org/
DefaultDirName={pf}\SIGViewer_2.3.2
DefaultGroupName=SIGViewer
OutputBaseFilename=setup
OutputDir=SIGViewerSetup
Compression=lzma
SolidCompression=yes
ChangesEnvironment=true
SetupIconFile=icon.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";
Name: modifypath; Description: &Add Java install directory to your environmental path; GroupDescription: "Settings:";

[Dirs]
Name: "{app}\SIGViewer/.ssh";Permissions: authusers-modify
Name: "{app}\SIGViewer/bin";Permissions: authusers-modify
Name: "{app}\SIGViewer/bin/shape";Permissions: users-full

[Files]
Source: "../Release_2010/SIGViewer.exe" ; DestDir:{app}/SIGViewer/bin;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/GitHub/x3d/jars/*" ; DestDir:{app}/SIGViewer/bin/Xj3D/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/GitHub/SIGViewer/SIGViewer/OgreSDK_additions/media/*" ; DestDir:{app}/SIGViewer/media_additions/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/GitHub/SIGViewer/SIGViewer/CEGUI_additions/datafiles/*" ; DestDir:{app}/SIGViewer/datafiles_additions/;  Flags: recursesubdirs createallsubdirs;permissions:users-full
Source: "C:/SIGVerse/GitHub/SIGViewer/SIGViewer/OculusResources/*" ; DestDir:{app}/SIGViewer/bin/media/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/ext/OgreSDK_vc10_v1-8-0/media/*" ; DestDir:{app}/SIGViewer/media/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/ext/OgreSDK_vc10_v1-8-0/bin/release/*.dll" ; DestDir:{app}/SIGViewer/bin/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/ext/CEGUI-0.7.6/datafiles/*" ; DestDir:{app}/SIGViewer/datafiles/;  Flags: recursesubdirs createallsubdirs;permissions:users-full
Source: "C:/SIGVerse/ext/CEGUI-0.7.6/bin/*.dll" ; DestDir:{app}/SIGViewer/bin/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/ext/zlib/bin/*.dll" ; DestDir:{app}/SIGViewer/bin/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "C:/SIGVerse/ext/libssh2-1.4.2-openssl-x86-win32nt-msvc/bin/*.dll" ; DestDir:{app}/SIGViewer/bin/;  Flags: recursesubdirs createallsubdirs; permissions:users-full
Source: "./licenses/*" ; DestDir:{app}/SIGViewer/licenses/;  Flags: recursesubdirs createallsubdirs;permissions:users-full
Source: "resources.cfg" ; DestDir:{app}/SIGViewer/bin; permissions:users-full
Source: "ogre.cfg" ; DestDir:{app}/SIGViewer/bin; permissions:users-full
Source: "plugins.cfg" ; DestDir:{app}/SIGViewer/bin; permissions:users-full
Source: "X3DParser.cfg" ; DestDir:{app}/SIGViewer/bin; permissions:users-full
Source: "SIGVerse.ini" ; DestDir:{app}/SIGViewer/bin; permissions:users-full

[Icons]
Name: "{group}\SIGViewer"; Filename: "{app}\SIGViewer\bin\SIGViewer.exe"; WorkingDir: "{app}\SIGViewer\bin"
Name: "{commondesktop}\SIGViewer 2.3.2"; Filename: "{app}\SIGViewer\bin\SIGViewer.exe"; WorkingDir: "{app}\SIGViewer\bin"; Tasks: desktopicon
; begin(add)(2010/2/24) install SIGWorldEditor.exe





[Code]
var
        VC2010RuntimePage: TInputOptionWizardPage;
        JREPage: TInputOptionWizardPage;
        jvmpath: string;  



procedure InitializeWizard;
begin
        // -----------------------------------------------
        //      VC++ Runtime Install Page
        // -----------------------------------------------
        VC2010RuntimePage := CreateInputOptionPage(
                wpWelcome,
                'Install Library for SIGViewer',
                'Visual C++ runtime install',
                'SIGViewer needs to install Visual C++ runtime. Do you accept it?',
                True, False);
        VC2010RuntimePage.Add('Yes. please proceed');
        VC2010RuntimePage.Add('No. skip VC++ runtime install');
        VC2010RuntimePage.Values[0]:=True;

        // -----------------------------------------------
        //      JRE Install Page
        // -----------------------------------------------
        JREPage := CreateInputOptionPage(
                VC2010RuntimePage.ID,
                'Install Library for SIGViewer',
                'JRE (Java Runtime Environment) install',
                'SIGViewer needs to install JRE 8.0 or later. Do you accept it?',
                True, False);
        JREPage.Add('Yes. please install JRE 8.0');
        JREPage.Add('No. I have already installed JRE 8.0 No need to install it again.');
        JREPage.Values[0]:=True;
        jvmpath := '{pf}\Java\jre1.8.0_66\bin\client\';
end;



// **************************************************************
//      Installation of Visual C++2010 runtime
// **************************************************************
function InstallVC2010Runtime: Boolean;
var
        rc: Integer;
        VC2010RuntimePath: String;
begin
        Result:=False;

        if VC2010RuntimePage.SelectedValueIndex = 0 then begin
                // ------------------------------------
                //      get path of the installer
                // ------------------------------------
                VC2010RuntimePath := ExpandConstant('{src}\downloads\vcredist_x86.exe');

                // ------------------------------------
                //      run installer
                // ------------------------------------
                if Exec(
                        VC2010RuntimePath,
                        '',
                        '',
                        SW_SHOW,
                        ewWaitUntilTerminated, rc) then begin
                        // exec succeeded. rc = return code
                end else begin
                        // exec failed. rc = error code
                        exit;
                end;
        end;

        Result:=True;
end;



// **************************************************************
//      Installation of JRE
// **************************************************************
function InstallJRE: Boolean;
var
        rc: Integer;
        JREInstellerPath: String;
begin
        if JREPage.SelectedValueIndex = 0 then begin
                // ------------------------------------
                //      get path of the installer
                // ------------------------------------
                JREInstellerPath := ExpandConstant('{src}\downloads\jre-8u66-windows-i586-iftw.exe');

                // ------------------------------------
                //      run installer
                // ------------------------------------
                if Exec(
                        JREInstellerPath,
                        '',
                        '',
                        SW_SHOW,
                        ewWaitUntilTerminated, rc) then
                begin
                        // ------------------------------------
                        // ------------------------------------
                        //JREPosPage.Values[0] := ExpandConstant('{pf}\Java\jre7');
                        

                //end else begin
                        // exec failed
                  //      exit;
                end;
        end else begin
                // ---------------------------------------------------
                // ---------------------------------------------------
                //JREPosPage.Values[0] := ExpandConstant('{pf}\Java');
        end;
        
        Result := True;
end;

// **************************************************************
// **************************************************************
function NextButtonClick(CurPageID: Integer): Boolean;
begin
        Result := False;
        if CurPageID = VC2010RuntimePage.ID then begin
                if not InstallVC2010Runtime then exit;
        end else if CurPageID = JREPage.ID then begin
                if not InstallJRE then exit;
        end;        

        Result := True;
end;

const
        ModPathName = 'modifypath';
        ModPathType = 'user';

function ModPathDir(): TArrayOfString;
begin
        setArrayLength(Result, 1);
        Result[0] := ExpandConstant(jvmpath);
end;
#include "modpath.iss"

[Run]
;Filename: "{app}/SIGViewer/Release/SIGViewer.exe"; Description: "{cm:LaunchProgram,SIGViewer}"; Flags: nowait postinstall skipifsilent
;Filename: "{app}/SIGViewer/Release/SIGViewer.exe"; Description: "{cm:LaunchProgram,SIGViewer}"; Flags: postinstall skipifsilent nowait

[UninstallDelete]
Type: filesandordirs; Name: "{app}\*"

