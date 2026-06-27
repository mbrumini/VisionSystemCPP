# VisionSystemCPP

## Italiano

Sistema di visione industriale C++20 / Qt Widgets / OpenCV per ricette
multi-camera, localizzazione pezzo, geometrie, misure, tolleranze e test
simulati con TestVision.

Il progetto e' in beta privata. Prima di lavorare leggere:

- `README.md`
- `TODO.md`
- `docs/helper_manual.md`
- `docs/source_file_map.md`

### Prerequisiti

- Windows 10/11
- Visual Studio 2022 con workload C++ desktop
- CMake 3.20+
- Qt 6 MSVC 2022 64 bit
- vcpkg con OpenCV installato per `x64-windows`
- facoltativo: Allied Vision Vimba X per camere Vimba

Variabili ambiente attese:

```powershell
QT_DIR=D:\Qt\6.10.3\msvc2022_64
VCPKG_ROOT=D:\dev\vcpkg
```

Sul PC di sviluppo principale puoi usare:

```powershell
.\usa_disco_d.ps1
```

### Build

Da PowerShell nella root del repo:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

Build mirata dell'HMI:

```powershell
cmake --build build --config Release --target VisionSystemCPP -- /m:1
```

Build del tool test:

```powershell
cmake --build build --config Release --target VisionSystemTestVision -- /m:1
```

Test automatici:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### Visual Studio 2022

Modo consigliato:

1. `File > Open > Folder...`
2. selezionare `D:\dev\VisionSystemCPP`
3. usare il preset `x64 Release`
4. scegliere come target di avvio `VisionSystemCPP`

Alternativa:

```text
D:\dev\VisionSystemCPP\build\VisionSystemCPP.sln
```

Non avviare `ALL_BUILD`: compila soltanto, non e' un eseguibile.

### Eseguibili

```text
build\Release\VisionSystemCPP.exe
build\Release\VisionSystemTestVision.exe
```

`VisionSystemCPP` e' l'app principale. `VisionSystemTestVision` invia frame
simulati e raccoglie report per prove aggressive.

### Struttura importante

```text
src/                  codice applicativo
config/               configurazione macchina/camere
recipes/              ricette prodotto e immagini campione versionabili
tests/scenarios/      scenari TestVision
tests/profiles/       profili TestVision
docs/                 manuali tecnici e help
translations/         testi UI it/en
```

### Cosa non committare

Sono locali e ignorati:

- `build/`, `deploy/`, `dist/`, `.vs/`
- `tests/reports/`
- log e dump (`*.log`, `*.dmp`, `*.mdmp`)
- dataset, training output e modelli AI generati
- acquisizioni reali grandi non ripulite

Le ricette demo piccole possono essere versionate. Dataset grandi o immagini
macchina reali vanno condivisi solo dopo decisione esplicita.

### Workflow Git

- lavorare su branch dedicato;
- prima di push: build `VisionSystemCPP`, build `VisionSystemTestVision`, `ctest`;
- non mischiare refactor grandi e fix piccoli nello stesso commit;
- documentare ogni nuovo tool in `docs/helper_manual.md`;
- aggiornare `TODO.md` quando una voce viene fatta o cambia priorita'.

### Stato attuale

La base e' buona per test privati:

- multicamera fino a 16 slot configurabili;
- sorgenti file, simulatore, USB, Vimba;
- geometrie configurabili, misure e tolleranze;
- geometrie costruite, inclusa linea media tra due linee;
- tool filetto in sviluppo;
- TestVision per stress test e confronto report.

Prima di coinvolgere utenti esterni al progetto, validare ancora su macchina reale.

---

## English

Industrial vision system prototype written in C++20 / Qt Widgets / OpenCV.
It supports multi-camera recipes, part localization, geometry tools,
measurements, tolerances, and simulated stress testing through TestVision.

The project is currently a private beta. Before working on it, read:

- `README.md`
- `TODO.md`
- `docs/helper_manual.md`

### Requirements

- Windows 10/11
- Visual Studio 2022 with Desktop C++ workload
- CMake 3.20+
- Qt 6 MSVC 2022 64 bit
- vcpkg with OpenCV installed for `x64-windows`
- optional: Allied Vision Vimba X for Vimba cameras

Expected environment variables:

```powershell
QT_DIR=D:\Qt\6.10.3\msvc2022_64
VCPKG_ROOT=D:\dev\vcpkg
```

On the main development PC you can run:

```powershell
.\usa_disco_d.ps1
```

### Build

From PowerShell in the repository root:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

Build only the main HMI:

```powershell
cmake --build build --config Release --target VisionSystemCPP -- /m:1
```

Build the test tool:

```powershell
cmake --build build --config Release --target VisionSystemTestVision -- /m:1
```

Run tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### Visual Studio 2022

Recommended mode:

1. `File > Open > Folder...`
2. select `D:\dev\VisionSystemCPP`
3. use the `x64 Release` preset
4. set `VisionSystemCPP` as startup target

Alternative:

```text
D:\dev\VisionSystemCPP\build\VisionSystemCPP.sln
```

Do not start `ALL_BUILD`: it is a build target, not an executable.

### Executables

```text
build\Release\VisionSystemCPP.exe
build\Release\VisionSystemTestVision.exe
```

`VisionSystemCPP` is the main application. `VisionSystemTestVision` sends
simulated frames and collects reports for aggressive tests.

### Important Directories

```text
src/                  application code
config/               machine/camera configuration
recipes/              product recipes and versioned sample images
tests/scenarios/      TestVision scenarios
tests/profiles/       TestVision profiles
docs/                 technical manuals and help files
translations/         UI text, Italian/English
```

### Do Not Commit

These are local/generated and ignored:

- `build/`, `deploy/`, `dist/`, `.vs/`
- `tests/reports/`
- logs and dumps (`*.log`, `*.dmp`, `*.mdmp`)
- generated AI datasets, training outputs, models
- large unreviewed real-machine captures

Small demo recipes can be versioned. Large datasets or real-machine images
must be shared only after an explicit decision.

### Git Workflow

- work on a dedicated branch;
- before pushing: build `VisionSystemCPP`, build `VisionSystemTestVision`, run `ctest`;
- do not mix large refactors and small fixes in the same commit;
- document each reusable tool/helper in `docs/helper_manual.md`;
- update `TODO.md` whenever a task is completed or reprioritized.

### Current State

The base is ready for private testing:

- up to 16 configurable camera slots;
- file, simulator, USB, and Vimba sources;
- configurable geometries, measurements, and tolerances;
- constructed geometries, including midline between two lines;
- thread inspection tool in progress;
- TestVision for stress tests and report comparison.

Before involving users outside the project, validate the system on the real
machine.
