# Regola disco D

Il disco `C:` e pieno: non installare pacchetti, cache, ambienti o dipendenze su `C:`.

Percorsi da usare:

- Temporanei: `D:\temp`
- Cache generiche: `D:\dev\.cache`
- vcpkg: `D:\dev\vcpkg`
- vcpkg binary cache: `D:\dev\.cache\vcpkg\binary`
- vcpkg downloads: `D:\dev\.cache\vcpkg\downloads`
- NuGet packages: `D:\dev\.cache\nuget\packages`
- pip cache: `D:\dev\.cache\pip`
- npm cache: `D:\dev\.cache\npm`
- Qt: `D:\Qt\6.10.3\msvc2022_64`
- aqtinstall virtualenv: `D:\dev\tools\aqt-venv`

Nota Visual Studio/MSVC:

- Qt e installato su `D:`.
- La compilazione con `msvc2022_64` richiede MSVC funzionante.
- Al momento Visual Studio risulta incompleto e richiede reboot; `cl.exe` non e presente nel toolset rilevato.
- Non riparare/installare Visual Studio su `C:` senza decisione esplicita.

Per applicare le variabili utente:

```powershell
.\usa_disco_d.ps1
```

Dopo l'esecuzione, aprire un nuovo terminale.

Questi percorsi sono specifici di questo PC. Il progetto condiviso usa le
variabili `QT_DIR` e `VCPKG_ROOT`, quindi sugli altri computer Qt e vcpkg
possono trovarsi su qualunque disco.

## Esecuzione GUI Qt

L'eseguibile in `build\Release` non va lanciato direttamente da doppio click se il `PATH` non contiene Qt, perche' Windows non trova DLL come `Qt6Widgets.dll`.

Per copiare le DLL Qt accanto all'exe di build e creare anche una cartella avviabile da Explorer:

```powershell
.\deploy_release.ps1
```

Poi lanciare:

```text
D:\dev\VisionSystemCPP\deploy\Release\VisionSystemCPP.exe
```

Dopo lo script funziona anche:

```text
D:\dev\VisionSystemCPP\build\Release\VisionSystemCPP.exe
```
