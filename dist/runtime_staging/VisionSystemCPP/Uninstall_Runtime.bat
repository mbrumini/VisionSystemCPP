@echo off
setlocal
set INSTALL_DIR=%~dp0
set SCRIPT=%~dp0installer\Uninstall-Runtime.ps1
if not exist "%SCRIPT%" set SCRIPT=%~dp0scripts\runtime\Uninstall-Runtime.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -InstallDir "%INSTALL_DIR%" %*
if errorlevel 1 (
  echo.
  echo Disinstallazione fallita. Premi un tasto per chiudere.
  pause >nul
  exit /b 1
)
echo.
echo Disinstallazione completata. Premi un tasto per chiudere.
pause >nul
