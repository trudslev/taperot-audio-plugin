; Inno Setup script for TapeRot's Windows installer (VST3 + Standalone - no AU, that's Apple-only).
; Version/output-filename/staging-directory are passed in from CI via /D defines; the fallbacks
; below only matter for a manual local run of `iscc TapeRot.iss` from this directory.
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef MyOutputBaseFilename
  #define MyOutputBaseFilename "TapeRot-installer"
#endif
#ifndef MyStageDir
  #define MyStageDir "stage"
#endif

[Setup]
; Fixed once shipped, like PLUGIN_CODE/BUNDLE_ID in CMakeLists.txt - changing it breaks upgrade
; detection for anyone who already installed a build with the old AppId.
AppId={{FB4BE57D-CB56-4F49-A6A1-1CC9C066B110}
AppName=TapeRot
AppVersion={#MyAppVersion}
AppPublisher=Neon Foundry
DefaultDirName={autopf}\TapeRot
DefaultGroupName=TapeRot
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
; VST3 goes into the shared Common Files location, so this always needs elevation.
PrivilegesRequired=admin
UninstallDisplayIcon={app}\TapeRot.exe

[Files]
Source: "{#MyStageDir}\Standalone\TapeRot.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyStageDir}\VST3\TapeRot.vst3\*"; DestDir: "{commoncf}\VST3\TapeRot.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Embedding a font in the binary is redistribution, and OFL 1.1 and Apache 2.0 both require the
; notice and licence to travel with it. The VST3 bundle carries its own copy from CMake; the
; Standalone is a bare .exe with no bundle, so this is its copy.
Source: "{#MyStageDir}\THIRD-PARTY-LICENCES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\TapeRot"; Filename: "{app}\TapeRot.exe"
Name: "{group}\Uninstall TapeRot"; Filename: "{uninstallexe}"

[UninstallDelete]
; {app} is removed automatically; the VST3 lives outside it (Common Files is shared across
; plugins) so it needs an explicit cleanup entry or uninstall would leave it behind.
Type: filesandordirs; Name: "{commoncf}\VST3\TapeRot.vst3"
