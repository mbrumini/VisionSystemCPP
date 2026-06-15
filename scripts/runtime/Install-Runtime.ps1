param(
  [string]$InstallDir = "C:\VisionSystemCPP",
  [string]$PackagePath = "",
  [string]$Repo = "mbrumini/VisionSystemCPP",
  [switch]$SkipPython,
  [switch]$InstallOllamaModel,
  [switch]$NoDesktopShortcut,
  [switch]$Force
)

$ErrorActionPreference = "Stop"

function Write-Step($Text) {
  Write-Host ""
  Write-Host "== $Text ==" -ForegroundColor Cyan
}

function Ensure-Directory($Path) {
  if (-not (Test-Path $Path)) {
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
  }
}

function Stop-App {
  Get-Process -Name "VisionSystemCPP" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Chiudo VisionSystemCPP aperto..."
    $_.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 2
    if (-not $_.HasExited) {
      $_.Kill()
    }
  }
}

function Get-LatestReleasePackage($RepoName, $DestinationDir) {
  $api = "https://api.github.com/repos/$RepoName/releases/latest"
  Write-Host "Cerco ultima release: $api"
  $release = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "VisionSystemCPP-Installer" }
  $asset = $release.assets | Where-Object { $_.name -like "VisionSystemCPP-runtime-*.zip" } | Select-Object -First 1
  if (-not $asset) {
    throw "Nessun asset VisionSystemCPP-runtime-*.zip trovato nell'ultima release GitHub."
  }
  $target = Join-Path $DestinationDir $asset.name
  Write-Host "Scarico $($asset.name)..."
  Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $target
  return $target
}

function Find-Python {
  $py = Get-Command py -ErrorAction SilentlyContinue
  if ($py) {
    return @{ Program = "py"; Args = @("-3.11") }
  }
  $python = Get-Command python -ErrorAction SilentlyContinue
  if ($python) {
    return @{ Program = "python"; Args = @() }
  }
  return $null
}

function Ensure-Python {
  $python = Find-Python
  if ($python) {
    return $python
  }

  $winget = Get-Command winget -ErrorAction SilentlyContinue
  if ($winget) {
    Write-Host "Python non trovato. Provo installazione automatica Python 3.11 con winget..."
    winget install --id Python.Python.3.11 -e --accept-source-agreements --accept-package-agreements
    $python = Find-Python
    if ($python) {
      return $python
    }
  }

  throw "Python 3.11 non trovato. Installare Python 3.11 e rilanciare l'installer."
}

function Create-Shortcut($ShortcutPath, $TargetPath, $WorkingDirectory) {
  $shell = New-Object -ComObject WScript.Shell
  $shortcut = $shell.CreateShortcut($ShortcutPath)
  $shortcut.TargetPath = $TargetPath
  $shortcut.WorkingDirectory = $WorkingDirectory
  $shortcut.IconLocation = $TargetPath
  $shortcut.Save()
}

$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)
Ensure-Directory $InstallDir
$logPath = Join-Path $InstallDir "install.log"
Start-Transcript -Path $logPath -Append | Out-Null

try {
  Write-Step "Installazione VisionSystemCPP"
  Write-Host "Cartella installazione: $InstallDir"

  Stop-App

  $tempRoot = Join-Path $env:TEMP ("VisionSystemCPP_install_" + [guid]::NewGuid().ToString("N"))
  Ensure-Directory $tempRoot

  if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    $PackagePath = Get-LatestReleasePackage $Repo $tempRoot
  }
  else {
    $PackagePath = [System.IO.Path]::GetFullPath($PackagePath)
  }

  if (-not (Test-Path $PackagePath)) {
    throw "Pacchetto non trovato: $PackagePath"
  }

  Write-Step "Estrazione pacchetto"
  $extractDir = Join-Path $tempRoot "extract"
  Expand-Archive -Force -Path $PackagePath -DestinationPath $extractDir

  $packageRoot = $extractDir
  $nestedExe = Get-ChildItem -Path $extractDir -Recurse -Filter "VisionSystemCPP.exe" | Select-Object -First 1
  if (-not $nestedExe) {
    throw "VisionSystemCPP.exe non trovato nel pacchetto."
  }
  $packageRoot = Split-Path -Parent $nestedExe.FullName

  $isUpdate = Test-Path (Join-Path $InstallDir "VisionSystemCPP.exe")
  if ($isUpdate) {
    Write-Host "Aggiornamento installazione esistente."
  }
  else {
    Write-Host "Nuova installazione."
  }

  Write-Step "Copia file runtime"
  $preserveDirs = @("config", "recipes", "data", "logs", ".venv-ai")
  $preserveFiles = @("config\cameras.json")

  Get-ChildItem -Path $packageRoot -Force | ForEach-Object {
    $name = $_.Name
    $dest = Join-Path $InstallDir $name
    if ($isUpdate -and $_.PSIsContainer -and $preserveDirs -contains $name -and (Test-Path $dest)) {
      Write-Host "Preservo cartella utente: $name"
      return
    }
    if ($isUpdate -and -not $_.PSIsContainer) {
      $relative = $name
      if ($preserveFiles -contains $relative -and (Test-Path $dest)) {
        Write-Host "Preservo file utente: $relative"
        return
      }
    }
    Copy-Item -Recurse -Force $_.FullName $InstallDir
  }

  # Copia config seed mancanti senza sovrascrivere configurazioni esistenti.
  $packageConfig = Join-Path $packageRoot "config"
  $installConfig = Join-Path $InstallDir "config"
  if (Test-Path $packageConfig) {
    Ensure-Directory $installConfig
    Get-ChildItem -Path $packageConfig -File | ForEach-Object {
      $dest = Join-Path $installConfig $_.Name
      if (-not (Test-Path $dest)) {
        Copy-Item -Force $_.FullName $dest
      }
    }
  }

  Ensure-Directory (Join-Path $InstallDir "data")
  Ensure-Directory (Join-Path $InstallDir "recipes")
  Ensure-Directory (Join-Path $InstallDir "logs")

  if (-not $SkipPython) {
    Write-Step "Ambiente Python AI"
    $python = Ensure-Python
    $venvPython = Join-Path $InstallDir ".venv-ai\Scripts\python.exe"
    if (-not (Test-Path $venvPython)) {
      & $python.Program @($python.Args + @("-m", "venv", (Join-Path $InstallDir ".venv-ai")))
    }
    & $venvPython -m pip install --upgrade pip
    & $venvPython -m pip install ultralytics opencv-python numpy
  }
  else {
    Write-Host "Installazione Python AI saltata."
  }

  if ($InstallOllamaModel) {
    Write-Step "Modello Ollama vision-help"
    $ollamaInstall = Join-Path $InstallDir "ollama\vision-help\install_vision_help.bat"
    if (Test-Path $ollamaInstall) {
      & $ollamaInstall
    }
    else {
      Write-Host "Script Ollama non trovato, salto."
    }
  }

  if (-not $NoDesktopShortcut) {
    Write-Step "Collegamenti"
    $desktop = [Environment]::GetFolderPath("Desktop")
    Create-Shortcut (Join-Path $desktop "VisionSystemCPP.lnk") (Join-Path $InstallDir "VisionSystemCPP.exe") $InstallDir
    $programs = [Environment]::GetFolderPath("Programs")
    $menuDir = Join-Path $programs "VisionSystemCPP"
    Ensure-Directory $menuDir
    Create-Shortcut (Join-Path $menuDir "VisionSystemCPP.lnk") (Join-Path $InstallDir "VisionSystemCPP.exe") $InstallDir
    Create-Shortcut (Join-Path $menuDir "Disinstalla VisionSystemCPP.lnk") (Join-Path $InstallDir "Uninstall_Runtime.bat") $InstallDir
  }

  $installedVersion = if (Test-Path (Join-Path $InstallDir "VERSION.txt")) { (Get-Content (Join-Path $InstallDir "VERSION.txt") -Raw).Trim() } else { "sconosciuta" }
  $manifest = [ordered]@{
    product = "VisionSystemCPP"
    version = $installedVersion
    installDir = $InstallDir
    installedAt = (Get-Date).ToString("s")
    package = $PackagePath
  }
  $manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $InstallDir "install_manifest.json")

  Write-Step "Completato"
  Write-Host "Versione installata: $installedVersion"
  Write-Host "Eseguibile: $(Join-Path $InstallDir "VisionSystemCPP.exe")"
}
finally {
  Stop-Transcript | Out-Null
}
