$ErrorActionPreference = "Stop"

$dependencies = @{
    VCPKG_ROOT = "E:\dev\vcpkg"
}

foreach ($entry in $dependencies.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Container)) {
        throw "Dipendenza non trovata per $($entry.Key): $($entry.Value)"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $dependencies.VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake") -PathType Leaf)) {
    throw "Installazione vcpkg non valida: toolchain non trovata in $($dependencies.VCPKG_ROOT)"
}

$paths = @{
    TEMP                       = "E:\temp"
    TMP                        = "E:\temp"
    NUGET_PACKAGES             = "E:\dev\.cache\nuget\packages"
    VCPKG_DEFAULT_BINARY_CACHE = "E:\dev\.cache\vcpkg\binary"
    VCPKG_DOWNLOADS            = "E:\dev\.cache\vcpkg\downloads"
    PIP_CACHE_DIR              = "E:\dev\.cache\pip"
    npm_config_cache           = "E:\dev\.cache\npm"
    CARGO_HOME                 = "E:\dev\.cache\cargo"
    RUSTUP_HOME                = "E:\dev\.cache\rustup"
}

foreach ($entry in $dependencies.GetEnumerator()) {
    [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
    Set-Item -Path "Env:$($entry.Key)" -Value $entry.Value
}

foreach ($entry in $paths.GetEnumerator()) {
    New-Item -ItemType Directory -Force -Path $entry.Value | Out-Null
    [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
    Set-Item -Path "Env:$($entry.Key)" -Value $entry.Value
}

# Qt standalone opzionale: su questo PC Qt arriva da vcpkg nel manifest.
$qtCandidates = @(
    "E:\Qt\6.10.3\msvc2022_64",
    (Join-Path $PSScriptRoot "build\vcpkg_installed\x64-windows")
)
foreach ($qtDir in $qtCandidates) {
    if (Test-Path -LiteralPath $qtDir -PathType Container) {
        [Environment]::SetEnvironmentVariable("QT_DIR", $qtDir, "User")
        Set-Item -Path "Env:QT_DIR" -Value $qtDir
        break
    }
}

Write-Host "vcpkg, cache e temporanei utente impostati su E:."
Write-Host "Apri un nuovo terminale per ereditare tutte le variabili."
