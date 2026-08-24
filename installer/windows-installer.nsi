; NSIS installer for NF Pro Eq (VST3, Windows)
; Built automatically by .github/workflows/build-windows.yml

!define PRODUCT_NAME "NF Pro Eq"
!define VST3_BUNDLE_NAME "NF Pro Eq.vst3"
!define VST3_SRC "..\build\NFEqualizer_artefacts\Release\VST3\NF Pro Eq.vst3"

Name "${PRODUCT_NAME}"
OutFile "${PRODUCT_NAME} Installer.exe"
InstallDir "$COMMONFILES64\VST3\${VST3_BUNDLE_NAME}"
RequestExecutionLevel admin

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "VST3 Plug-in"
  ; Clear out anything already at this exact path first. Older installer
  ; versions may have dropped a flat "<name>.vst3" FILE here (the pre-bundle
  ; VST3 format); if that file is still there, Windows can't create a folder
  ; with the same name, and every nested write below it fails with a cryptic
  ; "Error opening file for writing" dialog. Delete/RMDir are no-ops when the
  ; target doesn't match, so this is always safe to run.
  Delete "$INSTDIR"
  RMDir /r "$INSTDIR"

  SetOutPath "$INSTDIR"
  File /r "${VST3_SRC}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall ${PRODUCT_NAME}.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
SectionEnd
