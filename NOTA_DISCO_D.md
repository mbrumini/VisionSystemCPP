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
