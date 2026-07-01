param(
  [ValidateSet("CPU", "GPU")]
  [string]$Profile = "CPU",
  [string]$InstallDir = "C:\VisionSystemCPP",
  [string]$PythonVersion = "3.11",
  [string]$TorchCpuIndexUrl = "https://download.pytorch.org/whl/cpu",
  [string]$TorchGpuIndexUrl = "https://download.pytorch.org/whl/cu128",
  [switch]$ForceRecreateVenv
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

function Find-PythonLauncher {
  $py = Get-Command py -ErrorAction SilentlyContinue
  if ($py) {
    return @{ Program = "py"; Args = @("-$PythonVersion") }
  }

  $python = Get-Command python -ErrorAction SilentlyContinue
  if ($python) {
    return @{ Program = "python"; Args = @() }
  }

  return $null
}

function Ensure-Python {
  $python = Find-PythonLauncher
  if ($python) {
    return $python
  }

  $winget = Get-Command winget -ErrorAction SilentlyContinue
  if ($winget) {
    Write-Host "Python $PythonVersion non trovato. Provo installazione automatica con winget..."
    winget install --id "Python.Python.$PythonVersion" -e --accept-source-agreements --accept-package-agreements
    $python = Find-PythonLauncher
    if ($python) {
      return $python
    }
  }

  throw "Python $PythonVersion non trovato. Installare Python $PythonVersion e rilanciare."
}

function Invoke-Checked {
  param(
    [Parameter(Mandatory = $true)][string]$Program,
    [Parameter(Mandatory = $true)][string[]]$Arguments
  )

  Write-Host "> $Program $($Arguments -join ' ')"
  & $Program @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Comando fallito ($LASTEXITCODE): $Program $($Arguments -join ' ')"
  }
}

$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)
$logsDir = Join-Path $InstallDir "logs"
Ensure-Directory $logsDir
$logPath = Join-Path $logsDir ("vision_ai_{0}_install.log" -f $Profile.ToLowerInvariant())
Start-Transcript -Path $logPath -Append | Out-Null

try {
  Write-Step "VisionAI $Profile"
  Write-Host "Cartella applicazione: $InstallDir"
  Write-Host "Log: $logPath"

  if (-not (Test-Path (Join-Path $InstallDir "VisionSystemCPP.exe"))) {
    Write-Host "Avviso: VisionSystemCPP.exe non trovato in $InstallDir. Continuo comunque e preparo .venv-ai."
  }

  $python = Ensure-Python
  $venvDir = Join-Path $InstallDir ".venv-ai"
  $venvPython = Join-Path $venvDir "Scripts\python.exe"

  if ($ForceRecreateVenv -and (Test-Path $venvDir)) {
    Write-Step "Rimozione ambiente AI esistente"
    Remove-Item -Recurse -Force $venvDir
  }

  if (-not (Test-Path $venvPython)) {
    Write-Step "Creazione .venv-ai"
    Invoke-Checked -Program $python.Program -Arguments @($python.Args + @("-m", "venv", $venvDir))
  }
  else {
    Write-Host "Ambiente esistente: $venvDir"
  }

  Write-Step "Aggiornamento pip"
  Invoke-Checked -Program $venvPython -Arguments @("-m", "pip", "install", "--upgrade", "pip", "setuptools", "wheel")

  Write-Step "Installazione PyTorch $Profile"
  if ($Profile -eq "CPU") {
    Invoke-Checked -Program $venvPython -Arguments @(
      "-m", "pip", "install", "--upgrade",
      "torch", "torchvision", "torchaudio",
      "--index-url", $TorchCpuIndexUrl
    )
  }
  else {
    Invoke-Checked -Program $venvPython -Arguments @(
      "-m", "pip", "install", "--upgrade",
      "torch", "torchvision", "torchaudio",
      "--index-url", $TorchGpuIndexUrl
    )
  }

  Write-Step "Installazione Ultralytics e runtime visione"
  Invoke-Checked -Program $venvPython -Arguments @("-m", "pip", "install", "--upgrade", "ultralytics", "opencv-python", "numpy")

  Write-Step "Verifica ambiente"
  $check = @"
import sys
import cv2
import numpy
import torch
from ultralytics import YOLO
print("python=" + sys.version.split()[0])
print("torch=" + torch.__version__)
print("cuda_available=" + str(torch.cuda.is_available()))
print("cuda_device_count=" + str(torch.cuda.device_count()))
if torch.cuda.is_available():
    print("cuda_device=" + torch.cuda.get_device_name(0))
print("cv2=" + cv2.__version__)
print("numpy=" + numpy.__version__)
print("ultralytics_ok=true")
"@
  $checkPath = Join-Path $env:TEMP ("vision_ai_check_" + [guid]::NewGuid().ToString("N") + ".py")
  $check | Set-Content -Encoding UTF8 $checkPath
  try {
    Invoke-Checked -Program $venvPython -Arguments @($checkPath)
  }
  finally {
    Remove-Item -Force $checkPath -ErrorAction SilentlyContinue
  }

  if ($Profile -eq "GPU") {
    $cudaCheck = & $venvPython -c "import torch; print('true' if torch.cuda.is_available() else 'false')"
    if (($cudaCheck | Select-Object -Last 1).Trim() -ne "true") {
      throw "Profilo GPU installato, ma torch.cuda.is_available() e' false. Verificare GPU NVIDIA, driver e compatibilita' CUDA."
    }
  }

  Write-Step "Completato"
  Write-Host "Ambiente AI pronto: $venvPython" -ForegroundColor Green
}
finally {
  Stop-Transcript | Out-Null
}
