; Inno Setup script for the SPASynth Windows installer.
;
;   iscc /DAppVersion=1.0.0 /DArtefacts=..\..\build\SPASynth_artefacts\Release installers\windows\SPASynth.iss
;
; Code signing is opt-in: define SignTool on the command line to sign the
; installer and the installed binaries, e.g.
;   iscc "/DSignToolCmd=signtool sign /fd SHA256 /a $f" ...

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\build\SPASynth_artefacts\Release"
#endif

[Setup]
AppName=SPASynth
AppVersion={#AppVersion}
AppPublisher=Silverplatter Audio
AppPublisherURL=https://www.silverplatteraudio.com
DefaultDirName={commonpf64}\Silverplatter Audio\SPASynth
DefaultGroupName=Silverplatter Audio
DisableProgramGroupPage=yes
LicenseFile=..\..\packaging\docs\EULA.txt
OutputBaseFilename=SPASynth-{#AppVersion}-Windows
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#ifdef SignToolCmd
SignTool={#SignToolCmd}
#endif

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "app"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#Artefacts}\VST3\SPASynth.vst3\*"; DestDir: "{commoncf64}\VST3\SPASynth.vst3"; \
    Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Artefacts}\Standalone\SPASynth.exe"; DestDir: "{app}"; \
    Components: app; Flags: ignoreversion
Source: "..\..\packaging\docs\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\packaging\docs\QUICKSTART.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\packaging\docs\EULA.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SPASynth"; Filename: "{app}\SPASynth.exe"; Components: app
