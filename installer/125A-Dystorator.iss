; Inno Setup script for 125A Dystorator (Windows x64)
; Build: installer\make_package.ps1  (or ISCC.exe installer\125A-Dystorator.iss)

#define AppName "125A Dystorator"
#define AppVersion "0.1.0"
#define BuildDir GetEnv("USERPROFILE") + "\vab-build\Dystorator_artefacts\Release"

[Setup]
AppId={{D79B6F12-0E49-4DB4-9388-125AD1570A10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=125A Systems
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=125A-Dystorator-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#AppName}
CloseApplications=yes

[Files]
Source: "{#BuildDir}\VST3\125A Dystorator.vst3\*"; DestDir: "{commoncf64}\VST3\125A Dystorator.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Readme"; Filename: "{app}\README.txt"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
