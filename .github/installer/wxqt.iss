; Inno Setup script for the Windows installer of wxqt: installs the portable folder (dist/wxqt-portable) for the current user, as the program keeps its settings
; (Runnerwxqt.ini) and logs next to wxqt.exe, so it must live in a folder the user can write (not Program Files). Built by the workflow:
;   iscc /DAppVersion=0.1.0 .github/installer/wxqt.iss        (writes dist/wxqt-<version>-windows-x64-setup.exe)
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{E3A40CA0-772B-492A-B6B9-79F2A172C680}
AppName=wxqt
AppVersion={#AppVersion}
AppPublisher=wxqt
AppPublisherURL=https://github.com/intelati26/wxqt-grib
DefaultDirName={autopf}\wxqt
DefaultGroupName=wxqt
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\..\dist
OutputBaseFilename=wxqt-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\wxqt.exe
LicenseFile=..\..\COPYING

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "..\..\dist\wxqt-portable\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
; wxqt.bat puts the bundled GDAL on PATH before it starts wxqt.exe
Name: "{autoprograms}\wxqt"; Filename: "{app}\wxqt.bat"; WorkingDir: "{app}"; IconFilename: "{app}\wxqt.exe"; Flags: runminimized
Name: "{autodesktop}\wxqt"; Filename: "{app}\wxqt.bat"; WorkingDir: "{app}"; IconFilename: "{app}\wxqt.exe"; Flags: runminimized; Tasks: desktopicon

[Run]
Filename: "{app}\wxqt.bat"; Description: "Start wxqt"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent runminimized

[UninstallDelete]
; what the program writes next to itself
Type: filesandordirs; Name: "{app}\wxqt-run.log"
Type: files; Name: "{app}\wxqt.log"
