$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtBin = "D:\Qt\6.10.3\msvc2022_64\bin"
$buildExe = Join-Path $projectRoot "build\Release\VisionSystemCPP.exe"
$deployDir = Join-Path $projectRoot "deploy\Release"
$deployExe = Join-Path $deployDir "VisionSystemCPP.exe"

if (-not (Test-Path $buildExe)) {
    throw "Eseguibile non trovato: $buildExe. Compila prima con: cmake --build --preset x64-release"
}

New-Item -ItemType Directory -Force -Path $deployDir | Out-Null
Copy-Item -Force -Path $buildExe -Destination $deployExe

& (Join-Path $qtBin "windeployqt.exe") `
    --release `
    --compiler-runtime `
    --dir $deployDir `
    $deployExe

Write-Host "Deploy pronto:"
Write-Host $deployExe
