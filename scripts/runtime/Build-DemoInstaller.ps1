param(
  [string]$Configuration = "Release",
  [string]$QtBin = "",
  [string]$OutputDir = "",
  [string]$AppName = "VisionSystemCPP Demo",
  [string]$Publisher = "VisionSystemCPP",
  [switch]$IncludeRecipes,
  [switch]$SkipBuild,
  [switch]$NoRunAfterInstall
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

function Escape-Iss($Text) {
  return $Text.Replace("\", "\\").Replace('"', '""')
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $OutputDir = Join-Path $projectRoot "dist"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

if (-not $SkipBuild) {
  Write-Step "Build Release"
  & cmake --build --preset x64-release
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

$iss = @"
#define AppVersion "$version"
#define PackageRoot "$(Escape-Iss $packageRoot)"
#define AppIconFile "$(Escape-Iss (Join-Path $projectRoot "resources\app_icon.ico"))"

[Setup]
AppId={{7B2A5690-A781-40A2-92C9-273318C86C3D}
AppName=$AppName
AppVersion={#AppVersion}
AppPublisher=$Publisher
DefaultDirName=C:\VisionSystemCPP
DefaultGroupName=VisionSystemCPP
DisableProgramGroupPage=yes
OutputDir=$(Escape-Iss $OutputDir)
OutputBaseFilename=$setupBaseName
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayName=$AppName
UninstallDisplayIcon={app}\VisionSystemCPP.exe
SetupIconFile={#AppIconFile}
SetupLogging=yes

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "installai_cpu"; Description: "Installa VisionAI CPU completo (Python 3.11, PyTorch CPU, Ultralytics - richiede internet)"; GroupDescription: "Componenti AI opzionali:"; Flags: unchecked exclusive
Name: "installai_gpu"; Description: "Installa VisionAI GPU completo (NVIDIA/CUDA, PyTorch CUDA, Ultralytics - richiede internet)"; GroupDescription: "Componenti AI opzionali:"; Flags: unchecked exclusive
Name: "installollama"; Description: "Configura assistente help locale con Ollama"; GroupDescription: "Componenti aggiuntivi:"; Flags: unchecked

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "images,dataset,datasets,training,train,models,*.gguf,*.pt,*.onnx,*.bmp,*.jpg,*.jpeg,*.tif,*.tiff,*.webp"

[Dirs]
Name: "{app}\config"
Name: "{app}\recipes"
Name: "{app}\data"
Name: "{app}\logs"

[Icons]
Name: "{group}\VisionSystemCPP"; Filename: "{app}\VisionSystemCPP.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VisionSystemCPP.exe"
Name: "{group}\Disinstalla VisionSystemCPP"; Filename: "{app}\Uninstall_Runtime.bat"; WorkingDir: "{app}"
Name: "{autodesktop}\VisionSystemCPP"; Filename: "{app}\VisionSystemCPP.exe"; WorkingDir: "{app}"; IconFilename: "{app}\VisionSystemCPP.exe"; Tasks: desktopicon

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Install-VisionAI.ps1"" -Profile CPU -InstallDir ""{app}"""; StatusMsg: "Download e configurazione VisionAI CPU..."; Tasks: installai_cpu; Flags: runhidden waituntilterminated
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Install-VisionAI.ps1"" -Profile GPU -InstallDir ""{app}"""; StatusMsg: "Download e configurazione VisionAI GPU..."; Tasks: installai_gpu; Flags: runhidden waituntilterminated
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Install-Runtime.ps1"" -InstallDir ""{app}"" -SetupPythonOnly -SkipPython -InstallOllamaModel"; StatusMsg: "Configurazione dell'assistente Ollama..."; Tasks: installollama; Flags: runhidden waituntilterminated
Filename: "{app}\VisionSystemCPP.exe"; Description: "Avvia VisionSystemCPP"; WorkingDir: "{app}"; Flags: $runFlags

[UninstallRun]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\installer\Uninstall-Runtime.ps1"" -InstallDir ""{app}"" -Quiet"; RunOnceId: "VisionSystemCPPUninstallRuntime"; Flags: runhidden waituntilterminated
"@

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
