@echo off
setlocal
cd /d "%~dp0"
set SCRIPT=%~dp0installer\Install-Runtime.ps1
if not exist "%SCRIPT%" set SCRIPT=%~dp0scripts\runtime\Install-Runtime.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
if errorlevel 1 (
  echo.
  echo Installazione fallita. Premi un tasto per chiudere.
  pause >nul
  exit /b 1
)
echo.
echo Installazione completata. Premi un tasto per chiudere.
pause >nul
