$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
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

function Find-WindeployQt {
    $candidates = @()
    if ($env:QT_DIR) {
        $candidates += Join-Path $env:QT_DIR "bin\windeployqt.exe"
    }
    $candidates += @(
        (Join-Path $projectRoot "build\vcpkg_installed\x64-windows\tools\Qt6\bin\windeployqt.exe"),
        (Join-Path $projectRoot "build\vcpkg_installed\x64-windows\bin\windeployqt.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $fromPath = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    return $null
}

function Copy-RuntimeFromBuild {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDir,
        [Parameter(Mandatory = $true)][string]$TargetDir
    )

    Copy-Item -Force -Path (Join-Path $SourceDir "*.dll") -Destination $TargetDir -ErrorAction SilentlyContinue

    foreach ($pluginDir in @("platforms", "imageformats", "iconengines", "styles", "translations", "docs", "ollama")) {
        $source = Join-Path $SourceDir $pluginDir
        if (Test-Path $source) {
            Copy-Item -Recurse -Force -Path $source -Destination $TargetDir
        }
    }
}

function Find-VimbaApiBin {
    $candidates = @()
    if ($env:VIMBA_X_HOME) {
        $candidates += Join-Path $env:VIMBA_X_HOME "api\bin"
    }
    if ($env:VIMBAX_HOME) {
        $candidates += Join-Path $env:VIMBAX_HOME "api\bin"
    }
    $candidates += @(
        "C:\Program Files\Allied Vision\Vimba X\api\bin",
        "C:\Program Files\Allied Vision\VimbaX\api\bin"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Copy-VimbaRuntime {
    param(
        [Parameter(Mandatory = $true)][string]$TargetDir
    )

    $vimbaApiBin = Find-VimbaApiBin
    if (-not $vimbaApiBin) {
        Write-Host "Vimba X non trovato: salto copia DLL Vimba."
        return
    }

    Copy-Item -Force -Path (Join-Path $vimbaApiBin "*.dll") -Destination $TargetDir
    Write-Host "DLL Vimba copiate da: $vimbaApiBin"
}

New-Item -ItemType Directory -Force -Path $deployDir | Out-Null
Copy-Item -Force -Path $buildExe -Destination $deployExe

$windeployQt = Find-WindeployQt
if ($windeployQt) {
    & $windeployQt `
        --release `
        --compiler-runtime `
        --dir $buildDir `
        $buildExe
} else {
    Write-Host "windeployqt non trovato: uso le DLL gia' presenti in build\\Release."
}

New-Item -ItemType Directory -Force -Path $buildTranslationsDir | Out-Null
Copy-Item -Force -Path (Join-Path $translationsSource "*.json") -Destination $buildTranslationsDir
Copy-VimbaRuntime -TargetDir $buildDir
Copy-RuntimeFromBuild -SourceDir $buildDir -TargetDir $deployDir

if ($windeployQt) {
    & $windeployQt `
        --release `
        --compiler-runtime `
        --dir $deployDir `
        $deployExe
}

New-Item -ItemType Directory -Force -Path $deployTranslationsDir | Out-Null
Copy-Item -Force -Path (Join-Path $translationsSource "*.json") -Destination $deployTranslationsDir
Copy-VimbaRuntime -TargetDir $deployDir

Write-Host "Deploy pronto:"
Write-Host $deployExe
Write-Host "Anche l'eseguibile di build ora ha le DLL Qt accanto:"
Write-Host $buildExe
