param(
  [string]$PackageRoot = "",
  [string]$PackageZip = ""
)

$ErrorActionPreference = "Stop"

function Resolve-PackageRoot {
  if (-not [string]::IsNullOrWhiteSpace($PackageRoot)) {
    $resolved = Resolve-Path $PackageRoot
    return $resolved.Path
  }

  if ([string]::IsNullOrWhiteSpace($PackageZip)) {
    throw "Specificare -PackageRoot oppure -PackageZip."
  }

  $zip = Resolve-Path $PackageZip
  $extractRoot = Join-Path $env:TEMP ("VisionSystemCPP_validate_" + [guid]::NewGuid().ToString("N"))
  New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
  Expand-Archive -Force -Path $zip.Path -DestinationPath $extractRoot

  $exe = Get-ChildItem -Path $extractRoot -Recurse -Filter "VisionSystemCPP.exe" | Select-Object -First 1
  if (-not $exe) {
    throw "VisionSystemCPP.exe non trovato nello zip."
  }
  return (Split-Path -Parent $exe.FullName)
}

function Require-File($RelativePath) {
  $path = Join-Path $root $RelativePath
  if (-not (Test-Path $path)) {
    $script:errors += "Manca file: $RelativePath"
    return
  }
  $script:ok += "OK file: $RelativePath"
}

function Require-AnyFile($Description, [string[]]$RelativePaths) {
  foreach ($relative in $RelativePaths) {
    if (Test-Path (Join-Path $root $relative)) {
      $script:ok += "OK ${Description}: $relative"
      return
    }
  }
  $script:errors += "Manca ${Description}: $($RelativePaths -join ', ')"
}

function Warn-IfMissing($RelativePath, $Message) {
  if (-not (Test-Path (Join-Path $root $RelativePath))) {
    $script:warnings += $Message
  }
}

$root = Resolve-PackageRoot
$errors = @()
$warnings = @()
$ok = @()

Require-File "VisionSystemCPP.exe"
Require-File "Qt6Core.dll"
Require-File "Qt6Gui.dll"
Require-File "Qt6Widgets.dll"
Require-File "Qt6Svg.dll"
Require-File "platforms\qwindows.dll"
Require-File "imageformats\qsvg.dll"
Require-File "iconengines\qsvgicon.dll"
Require-File "opencv_core4.dll"
Require-File "opencv_imgproc4.dll"
Require-File "opencv_imgcodecs4.dll"
Require-File "translations\it.json"
Require-File "translations\en.json"
Require-File "config\app_settings.json"
Require-File "config\cameras.json"
Require-File "config\camera_profiles.json"
Require-File "INSTALLAZIONE_ALTRO_PC.txt"
Require-File "Install_Runtime.bat"
Require-File "Uninstall_Runtime.bat"
Require-File "VisionAI_CPU.bat"
Require-File "VisionAI_GPU.bat"
Require-File "installer\Install-Runtime.ps1"
Require-File "installer\Uninstall-Runtime.ps1"
Require-File "installer\Install-VisionAI.ps1"
Require-File "vcruntime140.dll"
Require-File "vcruntime140_1.dll"
Require-File "msvcp140.dll"

Warn-IfMissing "docs\help" "Help locale non trovato: la demo parte, ma l'assistente avra' meno contenuti."
Warn-IfMissing "recipes" "Ricette demo non incluse: usare Build-DemoInstaller.ps1 con -IncludeRecipes per una presentazione piu' completa."
Warn-IfMissing "ollama\vision-help" "Materiale Ollama non incluso: va bene se non si presenta l'help AI locale."

Write-Host ""
Write-Host "Validazione runtime VisionSystemCPP"
Write-Host "Root: $root"
Write-Host ""

foreach ($line in $ok) {
  Write-Host $line -ForegroundColor DarkGreen
}

foreach ($warning in $warnings) {
  Write-Host "WARN: $warning" -ForegroundColor Yellow
}

if ($errors.Count -gt 0) {
  Write-Host ""
  Write-Host "ERRORI:" -ForegroundColor Red
  foreach ($error in $errors) {
    Write-Host "- $error" -ForegroundColor Red
  }
  throw "Pacchetto runtime incompleto: $($errors.Count) errore/i."
}

Write-Host ""
Write-Host "Pacchetto runtime valido." -ForegroundColor Green
