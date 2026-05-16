$ErrorActionPreference = "Stop"

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

foreach ($entry in $paths.GetEnumerator()) {
    New-Item -ItemType Directory -Force -Path $entry.Value | Out-Null
    [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
    Set-Item -Path "Env:$($entry.Key)" -Value $entry.Value
}

Write-Host "Cache e temporanei utente impostati su D:."
Write-Host "Apri un nuovo terminale per ereditare tutte le variabili."
