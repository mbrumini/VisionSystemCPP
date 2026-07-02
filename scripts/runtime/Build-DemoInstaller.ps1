param(
  [string]$Configuration = "Release",
  [string]$QtBin = "",
  [string]$OutputDir = "",
  [string]$AppName = "VisionSystemCPP Demo",
  [string]$Publisher = "VisionSystemCPP",
  [switch]$IncludeRecipes,
  [switch]$SkipBuild,
  [switch]$NoRunAfterInstall,
  [switch]$SilentDefaults
)

$ErrorActionPreference = "Stop"

function Write-Step($Text) {
  Write-Host ""
  Write-Host "== $Text ==" -ForegroundColor Cyan
}

function Find-Iscc {
  $fromPath = Get-Command iscc.exe -ErrorAction SilentlyContinue
  if ($fromPath) {
    return $fromPath.Source
  }

  $candidates = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe",
    "D:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "D:\Program Files\Inno Setup 6\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 5\ISCC.exe",
    "C:\Program Files\Inno Setup 5\ISCC.exe",
    "D:\Program Files (x86)\Inno Setup 5\ISCC.exe",
    "D:\Program Files\Inno Setup 5\ISCC.exe"
  )
  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }
  return $null
}

function Find-Cmake {
  $fromPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
  if ($fromPath) {
    return $fromPath.Source
  }

  $candidates = @(
    "C:\Program Files\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }
  return $null
}

function Escape-Iss($Text) {
  return $Text.Replace("\", "\\").Replace('"', '""')
}

function Expand-IssTemplate {
  param(
    [Parameter(Mandatory = $true)][string]$TemplatePath,
    [Parameter(Mandatory = $true)][hashtable]$Values
  )

  $content = Get-Content -Raw -Encoding UTF8 $TemplatePath
  foreach ($key in $Values.Keys) {
    $content = $content.Replace("{{$key}}", [string]$Values[$key])
  }
  return $content
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $OutputDir = Join-Path $projectRoot "dist"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

if (-not $SkipBuild) {
  Write-Step "Build Release"
  $cmake = Find-Cmake
  if (-not $cmake) {
    throw "CMake non trovato. Installa CMake o Visual Studio con il componente CMake."
  }
  & $cmake --build --preset x64-release --target VisionSystemCPP
}

$versionPath = Join-Path $projectRoot "VERSION.txt"
$version = if (Test-Path $versionPath) { (Get-Content $versionPath -Raw).Trim() } else { "0.0.0" }

Write-Step "Creazione runtime demo"
$buildRuntime = Join-Path $PSScriptRoot "Build-RuntimePackage.ps1"
$runtimeArgs = @{
  Configuration = $Configuration
  OutputDir = $OutputDir
}
if (-not [string]::IsNullOrWhiteSpace($QtBin)) {
  $runtimeArgs.QtBin = $QtBin
}
if ($IncludeRecipes) {
  $runtimeArgs.IncludeRecipes = $true
}
& $buildRuntime @runtimeArgs

$packageRoot = Join-Path $OutputDir "runtime_staging\VisionSystemCPP"
$zipPath = Join-Path $OutputDir ("VisionSystemCPP-runtime-{0}.zip" -f $version)

Write-Step "Validazione runtime"
& (Join-Path $PSScriptRoot "Validate-RuntimePackage.ps1") -PackageRoot $packageRoot

Write-Step "Preparazione installer Inno Setup"
$installerWorkDir = Join-Path $OutputDir "installer"
New-Item -ItemType Directory -Force -Path $installerWorkDir | Out-Null

$setupBaseName = ("VisionSystemCPP-demo-setup-{0}" -f $version)
$issPath = Join-Path $installerWorkDir "VisionSystemCPP_Demo.iss"
$runFlags = "nowait postinstall skipifsilent"
if ($NoRunAfterInstall) {
  $runFlags += " unchecked"
}
if ($SilentDefaults) {
  $runFlags = "nowait skipifsilent unchecked"
}

$templatePath = Join-Path $PSScriptRoot "installer\VisionSystemCPP_Demo.iss.template"
if (-not (Test-Path $templatePath)) {
  throw "Template Inno Setup non trovato: $templatePath"
}
$iss = Expand-IssTemplate -TemplatePath $templatePath -Values @{
  APP_VERSION = $version
  PACKAGE_ROOT = Escape-Iss $packageRoot
  APP_ICON_FILE = Escape-Iss (Join-Path $projectRoot "resources\app_icon.ico")
  APP_NAME = $AppName
  PUBLISHER = $Publisher
  OUTPUT_DIR = Escape-Iss $OutputDir
  SETUP_BASE_NAME = $setupBaseName
  RUN_FLAGS = $runFlags
}

$iss | Set-Content -Encoding UTF8 $issPath

$iscc = Find-Iscc
if (-not $iscc) {
  Write-Host ""
  Write-Host "Inno Setup non trovato: installer .exe non compilato." -ForegroundColor Yellow
  Write-Host "Installa Inno Setup 6 e rilancia questo script, oppure apri/compila:"
  Write-Host $issPath
  Write-Host ""
  Write-Host "Runtime validato disponibile qui:"
  Write-Host $packageRoot
  Write-Host "Zip runtime:"
  Write-Host $zipPath
  exit 0
}

Write-Step "Compilazione installer"
& $iscc $issPath

$installerPath = Join-Path $OutputDir ("{0}.exe" -f $setupBaseName)
if (-not (Test-Path $installerPath)) {
  throw "Installer atteso non trovato: $installerPath"
}

$hash = Get-FileHash -Algorithm SHA256 $installerPath
"$($hash.Hash)  $(Split-Path -Leaf $installerPath)" | Set-Content -Encoding ASCII "$installerPath.sha256.txt"

Write-Host ""
Write-Host "Installer demo creato:" -ForegroundColor Green
Write-Host $installerPath
Write-Host "SHA256:"
Write-Host "$installerPath.sha256.txt"
exit 0
