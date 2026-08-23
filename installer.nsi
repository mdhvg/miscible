!define APP_NAME "Miscible"
!define MAIN_EXE "Miscible.exe"
!define PUBLISHER "mdhvg"

!ifndef APP_VERSION
    !error "APP_VERSION not passed"
!endif

; Windows requires strictly numeric X.X.X.X for VIProductVersion.
; but store full git version in ProductVersion and FileDescription!
!define WIN_PRODUCT_VERSION "2.0.0.0" 
VIProductVersion "${WIN_PRODUCT_VERSION}"

VIAddVersionKey /LANG=1033 "CompanyName" "mdhvg"
VIAddVersionKey /LANG=1033 "FileDescription" "Miscible Installer"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright © 2025-2026 Madhav Goyal"
VIAddVersionKey /LANG=1033 "LegalTrademarks" "miscible is a trademark of Madhav Goyal"
VIAddVersionKey /LANG=1033 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"

!define MUI_DPI_ADJUSTMENT 1

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
SetCompressor /SOLID lzma

Name "${APP_NAME}"
OutFile "${APP_NAME}_${APP_VERSION}_Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
RequestExecutionLevel admin

ManifestDPIAware true

BrandingText "miscible™ — Copyright © 2025-2026 Madhav Goyal"

!define MUI_ICON "data\Installer.ico"
!define MUI_UNICON "data\Uninstaller.ico"

!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_UNABOVETEXT

; 2. Top-Right Header Image for internal pages (150x57 .bmp)
!define MUI_HEADERIMAGE_BITMAP "data\header.bmp"
; !define MUI_HEADERIMAGE_UNBITMAP "data\header-un.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "data\banner.bmp"

!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_WELCOMEPAGE_TITLE "Welcome to the ${APP_NAME} Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of ${APP_NAME} ${APP_VERSION}.$\n$\nIt is recommended that you close all other applications before continuing.$\n$\nClick Next to continue."

!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_TEXT "${APP_NAME} has been installed on your computer.$\n$\nClick Finish to close this wizard."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${MAIN_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME} now"
!define MUI_LICENSEPAGE_CHECKBOX

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY

Page custom SetShortcutOptions LeaveShortcutOptions

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Var Checkbox_Desktop
Var Checkbox_StartMenu
Var Checkbox_VCRedist
Var Checkbox_Desktop_State
Var Checkbox_StartMenu_State
Var Checkbox_VCRedist_State
Var Dialog
Var NeedVCRedist

Function .onInit
    ClearErrors
    ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64" "Installed"
    IfErrors vc_missing
    IntCmp $0 1 vc_installed vc_missing vc_missing
    vc_missing:
        StrCpy $NeedVCRedist 1
        Return
    vc_installed:
        StrCpy $NeedVCRedist 0
FunctionEnd

Function SetShortcutOptions
    !insertmacro MUI_HEADER_TEXT "Installation Preferences" "Choose your preferred shortcuts and components."

    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateCheckbox} 15u 25u 230u 12u "Create a desktop shortcut"
    Pop $Checkbox_Desktop
    ${NSD_SetState} $Checkbox_Desktop 1

    ${NSD_CreateCheckbox} 15u 45u 230u 12u "Create a Start Menu shortcut"
    Pop $Checkbox_StartMenu
    ${NSD_SetState} $Checkbox_StartMenu 1

    ${If} $NeedVCRedist == 1
        ${NSD_CreateCheckbox} 15u 65u 250u 12u "Install Visual C++ Redistributable (Required for onnxruntime)"
        Pop $Checkbox_VCRedist
        ${NSD_SetState} $Checkbox_VCRedist 1
    ${EndIf}

    nsDialogs::Show
FunctionEnd

Function LeaveShortcutOptions
    ${NSD_GetState} $Checkbox_Desktop $Checkbox_Desktop_State
    ${NSD_GetState} $Checkbox_StartMenu $Checkbox_StartMenu_State

    ${If} $NeedVCRedist == 1
        ${NSD_GetState} $Checkbox_VCRedist $Checkbox_VCRedist_State
    ${Else}
        StrCpy $Checkbox_VCRedist_State 0
    ${EndIf}
FunctionEnd

Section "Install"
    SetOutPath "$INSTDIR"

    File "build/Miscible.exe"
    File "build/onnxruntime.dll"

    StrCmp $Checkbox_VCRedist_State 0 skip_vcredist
        DetailPrint "Installing bundled Visual C++ Redistributable..."
        File "build/vc_redist.x64.exe"
        ExecWait '"$INSTDIR\vc_redist.x64.exe" /quiet /norestart'
        Delete "$INSTDIR\vc_redist.x64.exe"
    skip_vcredist:

    WriteUninstaller "$INSTDIR\uninstall.exe"
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\${MAIN_EXE}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "EstimatedSize" $0

    StrCmp $Checkbox_StartMenu_State 0 skip_startmenu
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${MAIN_EXE}"
    skip_startmenu:

    StrCmp $Checkbox_Desktop_State 0 skip_desktop
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${MAIN_EXE}"
    skip_desktop:
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\Miscible.exe"
    Delete "$INSTDIR\onnxruntime.dll"
    Delete "$INSTDIR\uninstall.exe"

    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"

    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    RMDir "$INSTDIR"
SectionEnd
