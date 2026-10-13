@echo off
rem SSLM UE5.8 Editor startup repair entry point; use only the checked recovery script.
rem Does not delete UE assets, does not clean Engine binaries, and does not build Client/Server/Cook.
where pwsh.exe >nul 2>nul
if errorlevel 1 (
  echo [SSLM] PowerShell 7 not found. Open this script via an installed PowerShell 7.
  pause
  exit /b 2
)
start "SSLM UE5.8 Editor Startup Repair" pwsh.exe -NoProfile -NoLogo -NoExit -ExecutionPolicy Bypass -File "%~dp0RecoverUE58EditorForMaterialLibrary.ps1"
if errorlevel 1 exit /b %errorlevel%
exit /b 0
