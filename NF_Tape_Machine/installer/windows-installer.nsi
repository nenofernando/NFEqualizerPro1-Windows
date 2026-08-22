; NSIS installer for NF Tape Machine (VST3, Windows)
; Built automatically by .github/workflows/build-windows-tape-machine.yml

!include "MUI2.nsh"

!define PRODUCT_NAME "NF Tape Machine"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_PUBLISHER "NF Audio"
!define VST3_BUNDLE_NAME "NF Tape Machine.vst3"
!define VST3_SRC "..\build\NFTapeMachine_artefacts\Release\VST3\NF Tape Machine.vst3"
!define MANUAL_SRC "..\manual\NF_Tape_Machine_Manual.pdf"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME} Installer.exe"
InstallDir "$COMMONFILES64\VST3\${VST3_BUNDLE_NAME}"
RequestExecutionLevel admin

; File properties shown in Windows Explorer (right-click > Properties > Details)
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "© Neno Fernando"

!define MUI_WELCOMEPAGE_TITLE "Welcome to the ${PRODUCT_NAME} ${PRODUCT_VERSION} Setup"
!define MUI_WELCOMEPAGE_TEXT "This will install ${PRODUCT_NAME} version ${PRODUCT_VERSION} by ${PRODUCT_PUBLISHER}.$\r$\n$\r$\nFormat: VST3$\r$\nInstall location: $COMMONFILES64\VST3$\r$\n$\r$\nA PDF user manual is installed alongside the plug-in.$\r$\n$\r$\nClick Next to continue.$\r$\n$\r$\n© Neno Fernando"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE "${PRODUCT_NAME} ${PRODUCT_VERSION} installed"
!define MUI_FINISHPAGE_TEXT "Setup finished installing ${PRODUCT_NAME} ${PRODUCT_VERSION}.$\r$\n$\r$\nRescan your plug-ins (or restart your DAW) if it was open during installation."
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

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
