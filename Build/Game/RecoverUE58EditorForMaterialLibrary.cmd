@echo off
rem SSLM UE5.8 Editor startup repair entry point; use only the checked recovery script.
rem Safe default: start the existing Editor without triggering any new C++ build.
rem Does not delete UE assets, clean Engine binaries, or build Client/Server/Cook.
where pwsh.exe >nul 2>nul
if errorlevel 1 (
  echo [SSLM] PowerShell 7 not found. Open this script via an installed PowerShell 7.
  pause
  exit /b 2
)
rem Run synchronously so the caller can see the actual PowerShell/UE failure exit code.
pwsh.exe -NoProfile -NoLogo -ExecutionPolicy Bypass -File "%~dp0RecoverUE58EditorForMaterialLibrary.ps1" -SkipBuild
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
  echo [SSLM] Editor startup failed. Check the recovery report under Saved\Validation\MaterialLibrary\EditorStartupRecovery.
  pause
)
exit /b %RESULT%
