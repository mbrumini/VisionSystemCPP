$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtBin = "D:\Qt\6.10.3\msvc2022_64\bin"
$buildExe = Join-Path $projectRoot "build\Release\VisionSystemCPP.exe"
$buildDir = Split-Path -Parent $buildExe
$deployDir = Join-Path $projectRoot "deploy\Release"
$deployExe = Join-Path $deployDir "VisionSystemCPP.exe"
$translationsSource = Join-Path $projectRoot "translations"
$buildTranslationsDir = Join-Path $buildDir "translations"
$deployTranslationsDir = Join-Path $deployDir "translations"

if (-not (Test-Path $buildExe)) {
    throw "Eseguibile non trovato: $buildExe. Compila prima con: cmake --build --preset x64-release"
}

New-Item -ItemType Directory -Force -Path $deployDir | Out-Null
Copy-Item -Force -Path $buildExe -Destination $deployExe

& (Join-Path $qtBin "windeployqt.exe") `
    --release `
    --compiler-runtime `
    --dir $buildDir `
    $buildExe

New-Item -ItemType Directory -Force -Path $buildTranslationsDir | Out-Null
Copy-Item -Force -Path (Join-Path $translationsSource "*.json") -Destination $buildTranslationsDir

& (Join-Path $qtBin "windeployqt.exe") `
    --release `
    --compiler-runtime `
    --dir $deployDir `
    $deployExe

New-Item -ItemType Directory -Force -Path $deployTranslationsDir | Out-Null
Copy-Item -Force -Path (Join-Path $translationsSource "*.json") -Destination $deployTranslationsDir

Write-Host "Deploy pronto:"
Write-Host $deployExe
Write-Host "Anche l'eseguibile di build ora ha le DLL Qt accanto:"
Write-Host $buildExe
