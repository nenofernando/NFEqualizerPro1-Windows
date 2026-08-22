; NSIS installer for NF Tape Machine (VST3, Windows)
; Built automatically by .github/workflows/build-windows-tape-machine.yml

!define PRODUCT_NAME "NF Tape Machine"
!define VST3_BUNDLE_NAME "NF Tape Machine.vst3"
!define VST3_SRC "..\build\NFTapeMachine_artefacts\Release\VST3\NF Tape Machine.vst3"
!define MANUAL_SRC "..\manual\NF_Tape_Machine_Manual.pdf"

Name "${PRODUCT_NAME}"
OutFile "${PRODUCT_NAME} Installer.exe"
InstallDir "$COMMONFILES64\VST3\${VST3_BUNDLE_NAME}"
RequestExecutionLevel admin

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "VST3 Plug-in"
  SetOutPath "$INSTDIR"
  File /r "${VST3_SRC}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall ${PRODUCT_NAME}.exe"
SectionEnd

Section "User Manual"
  SetOutPath "$COMMONFILES64\VST3"
  File "${MANUAL_SRC}"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  Delete "$COMMONFILES64\VST3\NF_Tape_Machine_Manual.pdf"
SectionEnd
