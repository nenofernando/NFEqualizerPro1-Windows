; NF Resonance -- Windows VST3 TEST installer.
; Built by CI (GitHub Actions, windows-latest) against the real JUCE/VST3
; build output -- never a fabricated/placeholder binary. Explicitly a TEST
; build: no Authenticode signing (no certificate available), VST3 only
; (no AAX on Windows in this pipeline).
#define MyAppName "NF Resonance"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "NF Audio Tools - By Nenno Fernando"
; NFResonance.iss lives at NF_Resonance\installer\windows\ ; the CI workflow
; configures CMake with -B build at the WORKSPACE ROOT (a sibling of
; NF_Resonance, not inside it) -- so reaching it needs three levels up
; (windows -> installer -> NF_Resonance -> workspace root), not two.
#define MyVST3Source "..\..\..\build\NFResonance_artefacts\Release\VST3\NF Resonance.vst3"
#define MyManualsSource "..\..\Documentation\Manuals"

[Setup]
AppId={{B6E2B6C0-6C1A-4B7C-9B0B-NFRESONANCE01}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; No real install root for a VST3-only product -- {app} exists purely to
; host the uninstaller/manuals/registry entry; the actual plugin goes to
; the DAW-standard Common Files\VST3 location below via its own DestDir.
DefaultDirName={commonpf64}\NF Audio Tools\NF Resonance
DefaultGroupName=NF Audio Tools\NF Resonance
DisableProgramGroupPage=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir=Output
OutputBaseFilename=NF_Resonance_V1.0_TEST_Windows_x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName} ({#MyAppVersion}) [TEST]
; This is a TEST build -- do not claim it's the public/final release.
AppComments=INTERNAL TEST BUILD -- not for public distribution
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Files]
; The VST3 bundle is a whole directory tree (Contents\x86_64-win\...) --
; installed recursively as-is into the standard 64-bit Common Files\VST3
; location every VST3 host (REAPER included) scans by default.
Source: "{#MyVST3Source}\*"; DestDir: "{commoncf64}\VST3\NF Resonance.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Manuals -- copied to TWO places:
; 1) {app}\Manuals, for the Start Menu shortcuts below.
; 2) {userappdata}\NF Audio Tools\NF Resonance\Manual, the SAME per-user
;    folder (PresetManager::presetsRootFolder()'s parent -> "Manual") the
;    plugin's own in-app hamburger-menu "Manual (English)"/"Manual
;    (Portugues)" items look in -- juce::File::userApplicationDataDirectory
;    resolves to %APPDATA% on Windows, exactly like {userappdata} here, so
;    this is the identical lookup the macOS installer's postinstall script
;    performs, just via Inno Setup's own [Files] mechanism instead of a
;    script.
Source: "{#MyManualsSource}\NF_Resonance_Manual_EN_V1.0.pdf"; DestDir: "{app}\Manuals"; Flags: ignoreversion
Source: "{#MyManualsSource}\NF_Resonance_Manual_PT_V1.0.pdf"; DestDir: "{app}\Manuals"; Flags: ignoreversion
Source: "{#MyManualsSource}\NF_Resonance_Manual_EN_V1.0.pdf"; DestDir: "{userappdata}\NF Audio Tools\NF Resonance\Manual"; Flags: ignoreversion
Source: "{#MyManualsSource}\NF_Resonance_Manual_PT_V1.0.pdf"; DestDir: "{userappdata}\NF Audio Tools\NF Resonance\Manual"; Flags: ignoreversion

[Icons]
Name: "{group}\NF Resonance Manual (EN)"; Filename: "{app}\Manuals\NF_Resonance_Manual_EN_V1.0.pdf"
Name: "{group}\Manual NF Resonance (PT)"; Filename: "{app}\Manuals\NF_Resonance_Manual_PT_V1.0.pdf"
Name: "{group}\Uninstall NF Resonance"; Filename: "{uninstallexe}"

[UninstallDelete]
; Only OUR own plugin folder inside Common Files\VST3 is removed -- the
; shared VST3 directory itself, and every other publisher's plugin in it,
; is never touched.
Type: filesandordirs; Name: "{commoncf64}\VST3\NF Resonance.vst3"
