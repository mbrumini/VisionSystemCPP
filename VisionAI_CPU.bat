@echo off
setlocal
set INSTALL_DIR=%~dp0
if "%INSTALL_DIR:~-1%"=="\" set INSTALL_DIR=%INSTALL_DIR:~0,-1%
set SCRIPT=%~dp0installer\Install-VisionAI.ps1
if not exist "%SCRIPT%" set SCRIPT=%~dp0scripts\runtime\Install-VisionAI.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Profile CPU -InstallDir "%INSTALL_DIR%" %*
if errorlevel 1 (
  echo.
  echo Installazione VisionAI CPU fallita. Controlla logs\vision_ai_cpu_install.log.
  pause
  exit /b 1
)
echo.
echo VisionAI CPU pronto.
pause
