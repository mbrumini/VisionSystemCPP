# Manuale helper e tool interni

Questo documento serve a chi entra nel progetto per capire gli helper principali,
il motivo per cui esistono e quali regole rispettare quando si aggiunge codice.

English note: the detailed manual is in Italian. An English operational summary
is available at the end of this file.

Per una mappa rapida di tutti i file sorgente vedere `docs/source_file_map.md`.
Questa pagina resta focalizzata sugli helper e sui flussi principali.

## Principi

- La ricetta descrive il prodotto; `config/cameras.json` descrive la macchina.
- Ogni camera deve avere stato, runtime, risultati e ricetta camera separati.
- I tool devono lavorare sul frame corrente e, quando possibile, sulla posa pezzo
  corrente, non su coordinate schermo.
- La GUI raccoglie input e mostra overlay; la matematica deve stare in helper
  testabili e riusabili.
- I dati runtime generati non vanno committati.

## Configurazione camera

File principali:

- `src/config/AppConfig.h`
- `src/config/AppConfig.cpp`
- `config/cameras.json`

Responsabilita':

- caricare camere `CAM01...CAM16`;
- gestire sorgenti `file`, `simulator`, `usb`, `vimba`;
- salvare acquisizione, calibrazione, profilo e stato abilitato;
- filtrare le camere attive con `activeCameras()`.

Campi acquisizione importanti:

- `autoExposure`, `exposure`
- `autoGain`, `gain`
- `autoWhiteBalance`, `whiteBalance`
- `frameWidth`, `frameHeight` per forzare risoluzione USB;
- `frameIntervalMs` per il timer di acquisizione nel setup.

Se `frameWidth/frameHeight` sono assenti o zero, la USB prova la massima risoluzione
disponibile. Se sono valorizzati, viene richiesta quella risoluzione al driver.

## Runtime camera

File principali:

- `src/runtime/CameraRuntime.h`
- `src/runtime/CameraRuntime.cpp`
- `src/camera/ICamera.h`
- `src/camera/FileCamera.*`
- `src/camera/SimulatedCamera.*`
- `src/camera/UsbCamera.*`
- `src/camera/VimbaCamera.*`

`CameraRuntime` e' il contenitore runtime per una camera:

- apre/chiude la sorgente;
- conserva frame corrente, metadati simulatore, geometrie e posa;
- applica impostazioni live quando la sorgente lo supporta;
- espone `intervalMs()` usato dal timer setup.

Nota: per USB/Vimba i grab avvengono da timer nel setup; in START produzione la
pipeline deve restare asincrona per non bloccare la UI.

## USB camera

File:

- `src/camera/UsbCamera.cpp`
- `src/camera/UsbCameraDiscovery.cpp`

`UsbCameraDiscovery::probeBestResolution()` prova una lista di risoluzioni comuni
e mantiene quella col maggior numero di pixel effettivamente accettata dal driver.

`UsbCamera::open()` usa:

- risoluzione manuale se `frameWidth/frameHeight > 0`;
- altrimenti `probeBestResolution()`.

`UsbCamera::setAcquisitionSettings()` applica esposizione/gain/white balance e
puo' cambiare risoluzione live. Alcuni driver applicano davvero il formato solo
dopo riapertura dello stream: in quel caso fermare e riavviare la camera.

## Geometrie configurabili

File principali:

- `src/gui/modules/geometry/*`
- `src/config/GeometryRecipeJson.*`
- `src/processing/GeometryMeasurementPipeline.cpp`
- `src/processing/geometry/EdgeCircleDetector.*`
- `src/processing/geometry/EdgeCircleDetectorExperimental.*`

Le geometrie configurabili sono quelle disegnate/rilevate direttamente:

- punto;
- linea;
- cerchio;
- arco.

Regola: la configurazione sta nella ricetta camera; il risultato runtime sta nel
set geometrie della camera corrente.

Nota sul cerchio edge: `EdgeCircleDetectorExperimental` e' ora il detector
robusto ufficiale per cerchi e archi. Rimane separato dal detector storico per
permettere confronto e rollback rapido. Usa campionamento bilineare, selezione
bilanciata per settori e doppio fit con filtro residui.

Per forzare temporaneamente il detector storico da PowerShell:

```powershell
$env:VISION_EDGE_CIRCLE_STANDARD='1'
.\build\Release\VisionSystemCPP.exe
```

Per tornare al robusto ufficiale, chiudere il software e rilanciarlo senza quella
variabile oppure usare:

```powershell
Remove-Item Env:\VISION_EDGE_CIRCLE_STANDARD
```

## Geometrie costruite

File principali:

- `src/geometry/ConstructedGeometryMath.*`
- `src/gui/modules/MainWindowConstructedGeometryModule.*`
- `src/gui/modules/MainWindowConstructedGeometryPanels.cpp`
- `src/gui/modules/ConstructedGeometry*Source.*`

Le geometrie costruite derivano da altre geometrie gia' rilevate. Servono per
evitare misure duplicate e per creare riferimenti piu' stabili.

Tipi principali:

- punto medio tra due punti;
- intersezione tra due linee;
- proiezione punto-linea;
- linea offset;
- centro cerchio/arco;
- tangente da punto a cerchio;
- linea media tra due linee.

### Linea media tra due linee

Tipo ricetta: `midline_between_lines`.

Scopo: su pezzi simmetrici con gola/spallamento, invece di misurare sopra e sotto
con due misure quasi uguali, si crea una linea centrale tra le due linee limite.

La matematica:

- normalizza le due direzioni;
- allinea il verso delle linee;
- richiede linee quasi parallele;
- calcola la mezzeria tra i centri;
- usa la proiezione degli estremi per definire una linea abbastanza lunga.

Se le linee non sono parallele a sufficienza, la costruzione non deve inventare
un risultato: meglio stato non valido che una quota falsa.

## Misure

File principali:

- `src/gui/modules/MainWindowMeasurement*`
- `src/processing/GeometryMeasurementPipeline.cpp`
- `src/gui/MeasurementResultsWidget.*`

Le misure leggono geometrie runtime e producono valori in pixel o in unita' reali.
La conversione in mm deriva da:

- calibrazione camera, se presente;
- campione master/scala salvata, dove configurato.

Quando si crea una misura da GUI, le sorgenti selezionate vengono evidenziate
sull'immagine per ridurre errori di associazione.

## Filetto

File principali:

- `src/gui/modules/MainWindowThreadModule.*`
- ricetta camera: sezione `tools.threadInspection`

Il tool filetto e' verticale e ancora in affinamento. Misure gestite:

- diametro esterno/maggiore;
- diametro interno/minore;
- passo;
- fase;
- diametro medio/pitch diameter.

La fase confronta il riferimento angolare dei massimi del filetto esterno rispetto
all'asse di centro. Su campione in fase deve essere vicina a zero. Se due camere
simulano lo stesso campione e leggono la stessa fase non nulla, il problema e'
probabilmente nel campione o nella logica di accoppiamento dei picchi, non nella
multicamera.

## TestVision

File principali:

- `tests/testvision/*`
- `tests/scenarios/*`
- `tests/profiles/*`
- `tests/reports/` non versionato

TestVision invia frame simulati a VisionSystemCPP e raccoglie risultati. Serve per:

- stress test pose/rotazioni/traslazioni;
- confronto CAM01/CAM02/...;
- misure per angolo;
- diagnosi di regressioni.

In broadcast multicamera non usare `sendOnly`: la modalita' a raffica puo' saturare
la pipe. Il codice ora blocca questa combinazione e chiede raccolta dati.

## Help e documentazione operatore

File:

- `docs/help/it/functions/*.txt`
- `docs/help/it/index.txt`
- `docs/programmer_manual.md`

I file help sono testi brevi pensati per l'assistente interno e per l'operatore.
Questo manuale invece e' per sviluppatori/collaboratori.

## Regole per nuovi helper

- Mettere la matematica in file separati da GUI quando possibile.
- Aggiungere una nota qui se nasce un nuovo helper riusabile.
- Salvare nella ricetta solo configurazione, non risultati runtime.
- Non usare coordinate widget/schermo come dati persistenti.
- Aggiungere commenti solo sui passaggi non ovvi: formule, convenzioni, fallback.
- Aggiungere o aggiornare test quando si tocca una funzione condivisa.

---

# Internal Helpers And Tools - English Summary

This document explains the main internal helpers, why they exist, and the rules
to follow when adding new code. The detailed reference above is in Italian; this
section gives enough context for an English-speaking collaborator to work safely.

## Principles

- Recipes describe the product; `config/cameras.json` describes the machine.
- Each camera must have separate configuration, runtime state, results and recipe
  data.
- Tools should work on the current frame and, where possible, on the current part
  pose rather than on widget/screen coordinates.
- GUI code should collect input and show overlays. Reusable math should live in
  separate helpers that can be tested.
- Generated runtime data must not be committed.

## Camera Configuration

Main files:

- `src/config/AppConfig.h`
- `src/config/AppConfig.cpp`
- `config/cameras.json`

Important acquisition fields:

- `autoExposure`, `exposure`
- `autoGain`, `gain`
- `autoWhiteBalance`, `whiteBalance`
- `frameWidth`, `frameHeight` for manual USB resolution;
- `frameIntervalMs` for setup grab timer cadence.

If `frameWidth/frameHeight` are missing or zero, USB cameras try the highest
accepted resolution. If they are set, the driver is asked for that exact format.

## Camera Runtime

Main files:

- `src/runtime/CameraRuntime.*`
- `src/camera/ICamera.h`
- `src/camera/FileCamera.*`
- `src/camera/SimulatedCamera.*`
- `src/camera/UsbCamera.*`
- `src/camera/VimbaCamera.*`

`CameraRuntime` owns the active camera source, current frame, simulated metadata,
runtime geometries and part pose. It also exposes `intervalMs()`, used by the
setup grab timer.

## USB Camera

Main files:

- `src/camera/UsbCamera.cpp`
- `src/camera/UsbCameraDiscovery.cpp`

`UsbCameraDiscovery::probeBestResolution()` tries common resolutions and keeps
the largest format actually accepted by the driver.

`UsbCamera::open()` uses manual `frameWidth/frameHeight` when configured,
otherwise it probes the highest available resolution. Some USB drivers apply
resolution changes only after reopening the stream.

## Configurable Geometries

Main files:

- `src/gui/modules/geometry/*`
- `src/config/GeometryRecipeJson.*`
- `src/processing/GeometryMeasurementPipeline.cpp`
- `src/processing/geometry/EdgeCircleDetector.*`
- `src/processing/geometry/EdgeCircleDetectorExperimental.*`

Configurable geometries are directly drawn or detected: point, line, circle and
arc. Configuration lives in the camera recipe; runtime results live in the
current camera geometry set.

Circle edge note: `EdgeCircleDetectorExperimental` is now the official robust
detector for circles and arcs. It remains separate from the historical detector
so comparison and rollback stay simple. It uses bilinear sampling, balanced
sector selection and a two-pass fit with residual filtering.

To temporarily force the historical detector from PowerShell:

```powershell
$env:VISION_EDGE_CIRCLE_STANDARD='1'
.\build\Release\VisionSystemCPP.exe
```

To return to the official robust detector, close the software and relaunch it
without that variable, or run:

```powershell
Remove-Item Env:\VISION_EDGE_CIRCLE_STANDARD
```

## Constructed Geometries

Main files:

- `src/geometry/ConstructedGeometryMath.*`
- `src/gui/modules/MainWindowConstructedGeometryModule.*`
- `src/gui/modules/MainWindowConstructedGeometryPanels.cpp`
- `src/gui/modules/ConstructedGeometry*Source.*`

Constructed geometries are derived from detected geometries. They create stable
datums and avoid duplicated measurements.

Key examples:

- midpoint between two points;
- line-line intersection;
- point projection on line;
- offset line;
- circle/arc center;
- tangent from point to circle;
- midline between two lines.

`midline_between_lines` creates a datum halfway between two nearly parallel
lines. It rejects skewed line pairs instead of returning a plausible but false
reference.

## Measurements

Main files:

- `src/gui/modules/MainWindowMeasurement*`
- `src/processing/GeometryMeasurementPipeline.cpp`
- `src/gui/MeasurementResultsWidget.*`

Measurements read runtime geometries and produce values in pixels or real units.
Real-unit conversion comes from camera calibration or saved master/sample scale.
Source geometries are highlighted while creating measurements to reduce selection
mistakes.

## Thread Tool

Main files:

- `src/gui/modules/MainWindowThreadModule.*`
- camera recipe section `tools.threadInspection`

The thread tool is still being refined. It measures:

- major/external diameter;
- minor/internal diameter;
- pitch;
- phase;
- pitch diameter.

Thread phase compares external thread peaks against the center axis. On an
in-phase sample it should be close to zero. If two simulated cameras read the
same non-zero phase, the issue is likely in the sample or peak-pairing logic, not
in multicamera isolation.

## TestVision

Main files:

- `tests/testvision/*`
- `tests/scenarios/*`
- `tests/profiles/*`
- `tests/reports/` ignored

TestVision sends simulated frames to VisionSystemCPP and collects reports. Use it
for stress tests, camera comparisons, measurement-vs-angle checks and regression
diagnostics.

Do not use `sendOnly` for multicamera broadcast: it can saturate the pipe. The
application now blocks this combination.

## Rules For New Helpers

- Put reusable math outside GUI modules where practical.
- Add a note here when a reusable helper/tool is introduced.
- Save configuration in recipes, not runtime results.
- Never persist widget/screen coordinates as tool geometry.
- Comment only non-obvious formulas, conventions, fallbacks or industrial
  reasons.
- Add or update tests when shared behavior changes.
