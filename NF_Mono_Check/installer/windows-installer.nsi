; NSIS installer for NF Mono Check (VST3, Windows)
; Built automatically by .github/workflows/build-windows-monocheck.yml

!include "MUI2.nsh"

!define PRODUCT_NAME "NF Mono Check"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "NF Audio Tools"
!define PRODUCT_COPYRIGHT "NENNO FERNANDO AUDIO TOOLS(R) - All rights reserved."
!define VST3_BUNDLE_NAME "NF Mono Check.vst3"
!define VST3_SRC "..\build\NFMonoCheck_artefacts\Release\VST3\NF Mono Check.vst3"
!define MANUAL_EN_SRC "..\manual\NF_Mono_Check_Manual_EN.pdf"
!define MANUAL_PT_SRC "..\manual\NF_Mono_Check_Manual_PT.pdf"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "NF-Mono-Check-Windows-Installer.exe"
InstallDir "$COMMONFILES64\VST3\${VST3_BUNDLE_NAME}"
RequestExecutionLevel admin

; File properties shown in Windows Explorer (right-click > Properties > Details)
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "${PRODUCT_COPYRIGHT}"

!define MUI_WELCOMEPAGE_TITLE "Welcome to the ${PRODUCT_NAME} ${PRODUCT_VERSION} Setup"
!define MUI_WELCOMEPAGE_TEXT "This will install ${PRODUCT_NAME} version ${PRODUCT_VERSION} by ${PRODUCT_PUBLISHER}.$\r$\n$\r$\nFormat: VST3$\r$\nInstall location: $COMMONFILES64\VST3$\r$\n$\r$\nA bilingual PDF manual (English + Portuguese) is installed alongside the plug-in.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TITLE "${PRODUCT_NAME} ${PRODUCT_VERSION} installed"
!define MUI_FINISHPAGE_TEXT "Setup finished installing ${PRODUCT_NAME} ${PRODUCT_VERSION}.$\r$\n$\r$\nRescan your plug-ins (or restart your DAW) if it was open during installation.$\r$\n$\r$\n${PRODUCT_COPYRIGHT}"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "VST3 Plug-in"
  SetOutPath "$INSTDIR"
  File /r "${VST3_SRC}\*.*"

  SetOutPath "$PROGRAMFILES64\NF Audio Tools\NF Mono Check\Manual"
  File "/oname=NF Mono Check Manual (English).pdf" "${MANUAL_EN_SRC}"
  File "/oname=NF Mono Check Manual (Portugues).pdf" "${MANUAL_PT_SRC}"

  CreateDirectory "$SMPROGRAMS\NF Audio Tools\NF Mono Check"
  CreateShortCut "$SMPROGRAMS\NF Audio Tools\NF Mono Check\Manual (English).lnk" "$PROGRAMFILES64\NF Audio Tools\NF Mono Check\Manual\NF Mono Check Manual (English).pdf"
  CreateShortCut "$SMPROGRAMS\NF Audio Tools\NF Mono Check\Manual (Portugues).lnk" "$PROGRAMFILES64\NF Audio Tools\NF Mono Check\Manual\NF Mono Check Manual (Portugues).pdf"

  WriteUninstaller "$INSTDIR\Uninstall ${PRODUCT_NAME}.exe"
SectionEnd

Section "Uninstall"
  RMDir /r "$INSTDIR"
  RMDir /r "$PROGRAMFILES64\NF Audio Tools\NF Mono Check"
  RMDir /r "$SMPROGRAMS\NF Audio Tools\NF Mono Check"
SectionEnd
