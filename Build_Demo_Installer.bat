@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\runtime\Build-DemoInstaller.ps1" -IncludeRecipes %*
if errorlevel 1 (
  echo.
  echo Creazione installer demo fallita. Premi un tasto per chiudere.
  pause >nul
  exit /b 1
)
echo.
echo Processo completato. Premi un tasto per chiudere.
pause >nul
