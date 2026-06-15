@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\runtime\Build-RuntimePackage.ps1" %*
if errorlevel 1 (
  echo.
  echo Creazione pacchetto fallita. Premi un tasto per chiudere.
  pause >nul
  exit /b 1
)
echo.
echo Pacchetto creato. Premi un tasto per chiudere.
pause >nul
