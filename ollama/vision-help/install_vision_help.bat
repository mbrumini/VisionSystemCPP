@echo off
setlocal

where ollama >nul 2>nul
if errorlevel 1 (
  echo Ollama non trovato nel PATH. Installare Ollama e riprovare.
  exit /b 1
)

pushd "%~dp0"
ollama pull llama3.2:3b
if errorlevel 1 (
  popd
  exit /b 1
)

ollama create vision-help -f Modelfile
set RESULT=%ERRORLEVEL%
popd
exit /b %RESULT%
