param(
  [string]$Configuration = "Release",
  [string]$QtBin = "",
  [string]$OutputDir = "",
  [switch]$IncludeRecipes
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$versionPath = Join-Path $projectRoot "VERSION.txt"
$version = if (Test-Path $versionPath) { (Get-Content $versionPath -Raw).Trim() } else { "0.0.0" }
$buildDir = Join-Path $projectRoot "build\$Configuration"
$buildExe = Join-Path $buildDir "VisionSystemCPP.exe"

if (-not (Test-Path $buildExe)) {
  throw "Eseguibile non trovato: $buildExe. Compila sul PC sviluppo prima di creare il pacchetto."
}

function Find-WindeployQt {
  param([string]$QtBin)
  if ($QtBin -and (Test-Path (Join-Path $QtBin "windeployqt.exe"))) {
    return Join-Path $QtBin "windeployqt.exe"
  }
  if ($env:QT_DIR) {
    $candidate = Join-Path $env:QT_DIR "bin\windeployqt.exe"
    if (Test-Path $candidate) { return $candidate }
  }
  $candidates = @(
    (Join-Path $projectRoot "build\vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.exe"),
    (Join-Path $projectRoot "build\vcpkg_installed\x64-windows\bin\windeployqt.exe")
  )
  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) { return $candidate }
  }
  $fromPath = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
  if ($fromPath) { return $fromPath.Source }
  return $null
}

if ([string]::IsNullOrWhiteSpace($QtBin) -and $env:QT_DIR) {
  $QtBin = Join-Path $env:QT_DIR "bin"
}
$windeployqtPath = Find-WindeployQt $QtBin

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $OutputDir = Join-Path $projectRoot "dist"
}

function Copy-DirectoryFiltered {
  param(
    [Parameter(Mandatory = $true)][string]$SourceDir,
    [Parameter(Mandatory = $true)][string]$TargetDir,
    [string[]]$ExcludedDirectoryNames = @(),
    [string[]]$ExcludedFileExtensions = @(),
    [string[]]$ExcludedFileNames = @()
  )

  $sourceRoot = (Resolve-Path $SourceDir).Path
  New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null

  Get-ChildItem -Path $sourceRoot -Recurse -Force | ForEach-Object {
    $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
      return
    }

    $parts = $relativePath -split '[\\/]'
    foreach ($part in $parts) {
      if ($ExcludedDirectoryNames -contains $part) {
        return
      }
    }

    $destination = Join-Path $TargetDir $relativePath
    if ($_.PSIsContainer) {
      New-Item -ItemType Directory -Force -Path $destination | Out-Null
      return
    }

    if ($ExcludedFileExtensions -contains $_.Extension.ToLowerInvariant()) {
      return
    }
    if ($ExcludedFileNames -contains $_.Name.ToLowerInvariant()) {
      return
    }

    $destinationDir = Split-Path -Parent $destination
    New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    Copy-Item -Force $_.FullName $destination
  }
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

if ($windeployqtPath) {
  & $windeployqtPath `
    --release `
    --compiler-runtime `
    --dir $packageRoot `
    (Join-Path $packageRoot "VisionSystemCPP.exe")
} else {
  Write-Host "windeployqt non trovato: uso le DLL e i plugin di build/Release."
  # Copia le cartelle dei plugin Qt gia' presenti in build
  foreach ($pluginDir in @("platforms", "imageformats", "iconengines", "styles")) {
    $source = Join-Path $buildDir $pluginDir
    if (Test-Path $source) {
      Copy-Item -Recurse -Force $source (Join-Path $packageRoot $pluginDir)
    }
  }
}

# Copia DLL native gia' presenti accanto alla build, incluse OpenCV e runtime non gestiti da windeployqt.
Get-ChildItem -Path (Join-Path $buildDir "*.dll") -File | ForEach-Object {
  Copy-Item -Force $_.FullName (Join-Path $packageRoot $_.Name)
}

$runtimeDirs = @("translations", "resources", "vision_icon_pack", "tools", "docs", "ollama")
foreach ($dirName in $runtimeDirs) {
  $source = Join-Path $projectRoot $dirName
  if (Test-Path $source) {
    $target = Join-Path $packageRoot $dirName
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item -Recurse -Force (Join-Path $source "*") $target
  }
}

$configSource = Join-Path $projectRoot "config"
if (Test-Path $configSource) {
  $configTarget = Join-Path $packageRoot "config"
  New-Item -ItemType Directory -Force -Path $configTarget | Out-Null
  Copy-Item -Recurse -Force (Join-Path $configSource "*") $configTarget
}

if ($IncludeRecipes) {
  $recipesSource = Join-Path $projectRoot "recipes"
  if (Test-Path $recipesSource) {
    $recipesTarget = Join-Path $packageRoot "recipes"
    Copy-DirectoryFiltered `
      -SourceDir $recipesSource `
      -TargetDir $recipesTarget `
      -ExcludedDirectoryNames @("datasets", "models", "ai", "__pycache__") `
      -ExcludedFileExtensions @(".pyc") `
      -ExcludedFileNames @("results.png", "confusion_matrix.png", "confusion_matrix_normalized.png", "train_batch0.jpg", "train_batch1.jpg", "train_batch2.jpg", "val_batch0_labels.jpg", "val_batch0_pred.jpg", "val_batch1_labels.jpg", "val_batch1_pred.jpg", "val_batch2_labels.jpg", "val_batch2_pred.jpg")
  }
}

$msvcRuntimeNames = @(
  "vcruntime140.dll",
  "vcruntime140_1.dll",
  "msvcp140.dll",
  "concrt140.dll"
)
$msvcSearchDirs = @(
  $buildDir,
  (Join-Path $env:WINDIR "System32"),
  (Join-Path $env:WINDIR "SysWOW64")
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path $_) }

foreach ($runtimeName in $msvcRuntimeNames) {
  if (Test-Path (Join-Path $packageRoot $runtimeName)) {
    continue
  }
  $runtimeFile = $null
  foreach ($searchDir in $msvcSearchDirs) {
    $candidate = Join-Path $searchDir $runtimeName
    if (Test-Path $candidate) {
      $runtimeFile = $candidate
      break
    }
  }
  if ($runtimeFile) {
    Copy-Item -Force $runtimeFile (Join-Path $packageRoot $runtimeName)
  }
  else {
    Write-Host "Runtime MSVC non trovato localmente: $runtimeName"
  }
}

Copy-Item -Force (Join-Path $projectRoot "VERSION.txt") (Join-Path $packageRoot "VERSION.txt")
Copy-Item -Force (Join-Path $projectRoot "INSTALLAZIONE_ALTRO_PC.txt") (Join-Path $packageRoot "INSTALLAZIONE_ALTRO_PC.txt")

$installScripts = Join-Path $packageRoot "installer"
New-Item -ItemType Directory -Force -Path $installScripts | Out-Null
Copy-Item -Force (Join-Path $projectRoot "scripts\runtime\Install-Runtime.ps1") $installScripts
Copy-Item -Force (Join-Path $projectRoot "scripts\runtime\Uninstall-Runtime.ps1") $installScripts
Copy-Item -Force (Join-Path $projectRoot "scripts\runtime\Install-VisionAI.ps1") $installScripts
Copy-Item -Force (Join-Path $projectRoot "Install_Runtime.bat") $packageRoot
Copy-Item -Force (Join-Path $projectRoot "Uninstall_Runtime.bat") $packageRoot
Copy-Item -Force (Join-Path $projectRoot "VisionAI_CPU.bat") $packageRoot
Copy-Item -Force (Join-Path $projectRoot "VisionAI_GPU.bat") $packageRoot

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
