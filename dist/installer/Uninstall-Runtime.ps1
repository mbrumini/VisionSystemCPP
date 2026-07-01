param(
  [string]$InstallDir = "C:\VisionSystemCPP",
  [switch]$RemoveUserData,
  [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Remove-IfExists($Path) {
  if (Test-Path $Path) {
    Remove-Item -Recurse -Force $Path
  }
}

$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)

Get-Process -Name "VisionSystemCPP" -ErrorAction SilentlyContinue | ForEach-Object {
  $_.CloseMainWindow() | Out-Null
  Start-Sleep -Seconds 2
  if (-not $_.HasExited) {
    $_.Kill()
  }
}

if (-not $Quiet -and -not $RemoveUserData) {
  Write-Host "Disinstallazione VisionSystemCPP da: $InstallDir"
  Write-Host "Vuoi rimuovere anche ricette, immagini, modelli e configurazioni? (S/N)"
  $answer = Read-Host
  if ($answer -match "^[sS]") {
    $RemoveUserData = $true
  }
}

$desktop = [Environment]::GetFolderPath("Desktop")
$programs = [Environment]::GetFolderPath("Programs")
Remove-IfExists (Join-Path $desktop "VisionSystemCPP.lnk")
Remove-IfExists (Join-Path $programs "VisionSystemCPP")

if (-not (Test-Path $InstallDir)) {
  Write-Host "Cartella installazione non trovata."
  exit 0
}

if ($RemoveUserData) {
  Remove-IfExists $InstallDir
  Write-Host "VisionSystemCPP rimosso completamente."
  exit 0
}

$preserve = @("config", "recipes", "data", "logs")
Get-ChildItem -Path $InstallDir -Force | ForEach-Object {
  if ($preserve -contains $_.Name) {
    Write-Host "Conservo: $($_.Name)"
    return
  }
  Remove-Item -Recurse -Force $_.FullName
}

Write-Host "Applicazione rimossa. Dati utente conservati in: $InstallDir"
