Unicode True
RequestExecutionLevel admin

!ifndef VERSION
  !error "VERSION must be supplied"
!endif
!ifndef SOURCE
  !error "SOURCE must be supplied"
!endif
!ifndef OUTPUT
  !error "OUTPUT must be supplied"
!endif

Name "PRG32-QT ${VERSION}"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\PRG32-QT"
InstallDirRegKey HKLM "Software\PRG32-QT" "InstallDir"
SetCompressor /SOLID lzma

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "PRG32-QT" SEC_MAIN
  SetOutPath "$INSTDIR"
  File /r "${SOURCE}\*"
  WriteRegStr HKLM "Software\PRG32-QT" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRG32-QT" "DisplayName" "PRG32-QT"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRG32-QT" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRG32-QT" "Publisher" "PRG32-QT contributors"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRG32-QT" "UninstallString" "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\PRG32-QT"
  CreateShortcut "$SMPROGRAMS\PRG32-QT\PRG32-QT.lnk" "$INSTDIR\PRG32.exe"
  CreateShortcut "$SMPROGRAMS\PRG32-QT\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$SMPROGRAMS\PRG32-QT\PRG32-QT.lnk"
  Delete "$SMPROGRAMS\PRG32-QT\Uninstall.lnk"
  RMDir "$SMPROGRAMS\PRG32-QT"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "Software\PRG32-QT"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PRG32-QT"
SectionEnd
