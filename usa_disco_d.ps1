$ErrorActionPreference = "Stop"

$dependencies = @{
    QT_DIR     = "D:\Qt\6.10.3\msvc2022_64"
    VCPKG_ROOT = "D:\dev\vcpkg"
}

foreach ($entry in $dependencies.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Container)) {
        throw "Dipendenza non trovata per $($entry.Key): $($entry.Value)"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $dependencies.QT_DIR "bin\windeployqt.exe") -PathType Leaf)) {
    throw "Installazione Qt non valida: windeployqt.exe non trovato in $($dependencies.QT_DIR)"
}

if (-not (Test-Path -LiteralPath (Join-Path $dependencies.VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake") -PathType Leaf)) {
    throw "Installazione vcpkg non valida: toolchain non trovata in $($dependencies.VCPKG_ROOT)"
}

$paths = @{
    TEMP                       = "D:\temp"
    TMP                        = "D:\temp"
    NUGET_PACKAGES             = "D:\dev\.cache\nuget\packages"
    VCPKG_DEFAULT_BINARY_CACHE = "D:\dev\.cache\vcpkg\binary"
    VCPKG_DOWNLOADS            = "D:\dev\.cache\vcpkg\downloads"
    PIP_CACHE_DIR              = "D:\dev\.cache\pip"
    npm_config_cache           = "D:\dev\.cache\npm"
    CARGO_HOME                 = "D:\dev\.cache\cargo"
    RUSTUP_HOME                = "D:\dev\.cache\rustup"
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

Write-Host "Qt, vcpkg, cache e temporanei utente impostati su D:."
Write-Host "Apri un nuovo terminale per ereditare tutte le variabili."
