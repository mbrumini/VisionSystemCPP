param(
  [string]$Configuration = "Release",
  [string]$QtBin = "",
  [string]$OutputDir = "",
  [switch]$IncludeRecipes
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($QtBin) -and $env:QT_DIR) {
  $QtBin = Join-Path $env:QT_DIR "bin"
}
if ([string]::IsNullOrWhiteSpace($QtBin)) {
  throw "Qt non configurato. Imposta QT_DIR oppure passa -QtBin."
}
$versionPath = Join-Path $projectRoot "VERSION.txt"
$version = if (Test-Path $versionPath) { (Get-Content $versionPath -Raw).Trim() } else { "0.0.0" }
$buildDir = Join-Path $projectRoot "build\$Configuration"
$buildExe = Join-Path $buildDir "VisionSystemCPP.exe"
$windeployqt = Join-Path $QtBin "windeployqt.exe"

if (-not (Test-Path $buildExe)) {
  throw "Eseguibile non trovato: $buildExe. Compila sul PC sviluppo prima di creare il pacchetto."
}
if (-not (Test-Path $windeployqt)) {
  throw "windeployqt non trovato: $windeployqt"
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $OutputDir = Join-Path $projectRoot "dist"
}

$stagingRoot = Join-Path $OutputDir "runtime_staging"
$packageRoot = Join-Path $stagingRoot "VisionSystemCPP"
$zipPath = Join-Path $OutputDir ("VisionSystemCPP-runtime-{0}.zip" -f $version)
$shaPath = "$zipPath.sha256.txt"

if (Test-Path $stagingRoot) {
  Remove-Item -Recurse -Force $stagingRoot
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Copy-Item -Force $buildExe (Join-Path $packageRoot "VisionSystemCPP.exe")

& $windeployqt `
  --release `
  --compiler-runtime `
  --dir $packageRoot `
  (Join-Path $packageRoot "VisionSystemCPP.exe")

# Copia DLL native gia' presenti accanto alla build, incluse OpenCV e runtime non gestiti da windeployqt.
Get-ChildItem -Path (Join-Path $buildDir "*.dll") -File | ForEach-Object {
  Copy-Item -Force $_.FullName (Join-Path $packageRoot $_.Name)
}

$runtimeDirs = @("translations", "resources", "tools", "docs", "ollama")
foreach ($dirName in $runtimeDirs) {
  $source = Join-Path $projectRoot $dirName
  if (Test-Path $source) {
    Copy-Item -Recurse -Force $source (Join-Path $packageRoot $dirName)
  }
}

$configSource = Join-Path $projectRoot "config"
if (Test-Path $configSource) {
  Copy-Item -Recurse -Force $configSource (Join-Path $packageRoot "config")
}

if ($IncludeRecipes) {
  $recipesSource = Join-Path $projectRoot "recipes"
  if (Test-Path $recipesSource) {
    Copy-Item -Recurse -Force $recipesSource (Join-Path $packageRoot "recipes")
  }
}

Copy-Item -Force (Join-Path $projectRoot "VERSION.txt") (Join-Path $packageRoot "VERSION.txt")
Copy-Item -Force (Join-Path $projectRoot "INSTALLAZIONE_ALTRO_PC.txt") (Join-Path $packageRoot "INSTALLAZIONE_ALTRO_PC.txt")

$installScripts = Join-Path $packageRoot "installer"
New-Item -ItemType Directory -Force -Path $installScripts | Out-Null
Copy-Item -Force (Join-Path $projectRoot "scripts\runtime\Install-Runtime.ps1") $installScripts
Copy-Item -Force (Join-Path $projectRoot "scripts\runtime\Uninstall-Runtime.ps1") $installScripts
Copy-Item -Force (Join-Path $projectRoot "Install_Runtime.bat") $packageRoot
Copy-Item -Force (Join-Path $projectRoot "Uninstall_Runtime.bat") $packageRoot

# Rimuove cache Python accidentalmente copiate.
Get-ChildItem -Path $packageRoot -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
Get-ChildItem -Path $packageRoot -Recurse -File -Include *.pyc | Remove-Item -Force

$manifest = [ordered]@{
  product = "VisionSystemCPP"
  version = $version
  createdAt = (Get-Date).ToString("s")
  includeRecipes = [bool]$IncludeRecipes
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $packageRoot "runtime_package.json")

if (Test-Path $zipPath) {
  Remove-Item -Force $zipPath
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$created = $false
for ($attempt = 1; $attempt -le 5 -and -not $created; ++$attempt) {
  try {
    [System.GC]::Collect()
    [System.GC]::WaitForPendingFinalizers()
    Start-Sleep -Milliseconds (400 * $attempt)
    if (Test-Path $zipPath) {
      Remove-Item -Force $zipPath
    }
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
      $packageRoot,
      $zipPath,
      [System.IO.Compression.CompressionLevel]::Optimal,
      $false)
    $created = $true
  }
  catch {
    if ($attempt -eq 5) {
      throw
    }
    Write-Host "Zip non riuscito, ritento ($attempt/5): $($_.Exception.Message)"
  }
}

$hash = Get-FileHash -Algorithm SHA256 $zipPath
"$($hash.Hash)  $(Split-Path -Leaf $zipPath)" | Set-Content -Encoding ASCII $shaPath

Write-Host ""
Write-Host "Pacchetto runtime creato:"
Write-Host $zipPath
Write-Host "SHA256:"
Write-Host $shaPath
