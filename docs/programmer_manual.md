# VisionSystemCPP - Manuale programmatore

## Indice

1. Scopo del progetto
2. Struttura directory
3. Build, deploy e disco D
4. Configurazione camere
5. GUI principale
6. Traduzioni
7. Catalogo tool e pannelli placeholder
8. Ricette e prossimi passi
9. Mappa rapida dei file

## 1. Scopo del progetto

VisionSystemCPP e' un prototipo C++20/OpenCV/Qt per un sistema di visione industriale multi-camera.

L'obiettivo e' costruire un'applicazione manutenibile, modulare e pronta per:

- 16 camere configurabili;
- simulazione da immagini su disco;
- futura integrazione camere reali;
- GUI operatore in Qt Widgets;
- tool per camera;
- ricette prodotto;
- localizzazione e misure con OpenCV.

Principio guida: ogni camera deve avere configurazione, stato, processing e tool propri.

## 2. Struttura directory

```text
VisionSystemCPP/
|-- CMakeLists.txt
|-- CMakePresets.json
|-- config/
|   |-- cameras.json
|-- data/
|   |-- images/
|-- docs/
|   |-- programmer_manual.md
|-- src/
|   |-- camera/
|   |-- config/
|   |-- gui/
|   |-- processing/
|   |-- utils/
|-- translations/
|   |-- it.json
|   |-- en.json
|-- deploy_release.ps1
|-- usa_disco_d.ps1
```

I file generati da build e deploy non vanno versionati:

- `build/`
- `deploy/`
- `.vs/`

## 3. Build, Deploy E Disco D

Il disco C non deve essere usato per pacchetti, cache o dipendenze del progetto.

Percorsi principali:

- Qt: `D:\Qt\6.10.3\msvc2022_64`
- vcpkg: `D:\dev\vcpkg`
- progetto: `D:\dev\VisionSystemCPP`
- cache: `D:\dev\.cache`
- temporanei: `D:\temp`

Prima di lavorare:

```powershell
.\usa_disco_d.ps1
```

Build:

```powershell
cmake --build --preset x64-release
```

Deploy DLL Qt e traduzioni:

```powershell
.\deploy_release.ps1
```

Eseguibili avviabili:

```text
D:\dev\VisionSystemCPP\build\Release\VisionSystemCPP.exe
D:\dev\VisionSystemCPP\deploy\Release\VisionSystemCPP.exe
```

## 4. Configurazione Camere

File:

```text
config/cameras.json
```

Contiene:

- `system.maxCameras`
- lista camere `CAM01` ... `CAM16`
- `slot` numerico da 1 a 16
- `exists`
- `enabled`
- `type`
- `folder`
- `processingProfile`
- profili con `imageMode`, `inspectionTypes`, `guiTools`

Codice:

- `src/config/AppConfig.h`
- `src/config/AppConfig.cpp`
- `src/config/RecipeManager.h`
- `src/config/RecipeManager.cpp`

Responsabilita':

- caricare `config/cameras.json`;
- creare `CameraConfig`;
- filtrare le camere attive con `activeCameras()`;
- associare ogni camera al suo `ProcessingProfile`.

`RecipeManager` gestisce i dati di processo salvati nella ricetta attiva.

Prima funzione gestita:

- ROI/AOE di localizzazione per camera;
- salvataggio in `recipes/default/cameras/CAMxx.json`;
- coordinate salvate in pixel dell'immagine originale, non in coordinate schermo.

Gestione ricette dal menu:

- `Ricette -> Seleziona ricetta`: cambia ricetta attiva;
- `Ricette -> Nuova ricetta`: crea una nuova cartella sotto `recipes/`;
- `Ricette -> Duplica ricetta`: copia la ricetta attiva in una nuova ricetta;
- `Ricette -> Importa ricetta`: importa una directory ricetta esterna;
- `Ricette -> Esporta ricetta`: esporta la ricetta attiva come cartella.

La ricetta attiva viene salvata in:

```text
config/app_settings.json
```

## 5. GUI Principale

File:

- `src/main.cpp`
- `src/gui/MainWindow.h`
- `src/gui/MainWindow.cpp`
- `src/gui/CameraTileWidget.h`
- `src/gui/CameraTileWidget.cpp`
- `src/gui/ImageViewWidget.h`
- `src/gui/ImageViewWidget.cpp`

`main.cpp` crea `QApplication`, apre `MainWindow` e usa `showMaximized()`.

`MainWindow` gestisce:

- menu superiore;
- area immagini a sinistra;
- griglia miniature;
- vista camera selezionata;
- pannello destro;
- comandi generali;
- log eventi;
- tool dinamici della camera.

Il menu `Sistema` contiene anche il comando `Fullscreen / finestra`, che alterna tra:

- `showFullScreen()`: copre anche la barra Windows, utile per uso macchina/HMI;
- `showMaximized()`: comodo per sviluppo e debug.

`CameraTileWidget` gestisce una miniatura camera:

- immagine preview;
- nome camera;
- profilo;
- badge numerico in basso a destra;
- stato selezionato/non selezionato.

`ImageViewWidget` gestisce l'immagine grande:

- disegno immagine scalata;
- conversione coordinate mouse/widget in coordinate immagine originale;
- overlay ROI;
- drag mouse per disegnare AOE/ROI;
- callback quando la ROI cambia.

Nota layout:

- la griglia e' dentro `QScrollArea`;
- il pannello destro e' dentro `QScrollArea`;
- le miniature sono tenute uniformi per evitare righe di dimensioni diverse o contenuti fuori schermo.

## 6. Traduzioni

File:

- `translations/it.json`
- `translations/en.json`
- `src/config/TranslationManager.h`
- `src/config/TranslationManager.cpp`

Il `TranslationManager` carica i testi da JSON.

Ordine di ricerca:

1. cartella dell'eseguibile: `translations/<lingua>.json`;
2. fallback sviluppo: `PROJECT_SOURCE_DIR/translations/<lingua>.json`.

Uso tipico:

```cpp
trText("menu.recipes")
trText("commands.start")
trText("tools.measurements")
```

Il menu `Lingua` permette di cambiare tra italiano e inglese.

Regola: quando un testo GUI diventa stabile, deve passare dal JSON traduzioni.

## 7. Catalogo Tool E Pannelli Placeholder

File:

- `src/gui/ToolCatalog.h`
- `src/gui/ToolCatalog.cpp`
- `src/gui/ToolPanelWidget.h`
- `src/gui/ToolPanelWidget.cpp`

`ToolCatalog` definisce gli ID dei tool e delle azioni.

Esempio:

- `measurements`
- azioni: `diameter`, `length`, `radius`, `distance`, `angle`, `center`, `area`

Le etichette non stanno nel catalogo come testo definitivo: sono chiavi verso `translations/*.json`.

`ToolPanelWidget` crea il pannello che appare quando si clicca un tool.

Flusso:

1. selezione camera;
2. lettura `guiTools` dal profilo camera;
3. click su un tool;
4. apertura del pannello placeholder;
5. click su azione;
6. scrittura evento nel log.

Le funzioni operative reali verranno collegate a questi placeholder.

## 8. Ricette E Prossimi Passi

La configurazione hardware camera resta in:

```text
config/cameras.json
```

La configurazione processo/prodotto andra' in:

```text
recipes/
```

Struttura proposta:

```text
recipes/default/recipe.json
recipes/default/cameras/CAM01.json
recipes/default/cameras/CAM02.json
...
recipes/default/assets/
```

Per camere BW, il primo tool operativo importante sara':

- localizzazione pezzo;
- centro di massa;
- assi X/Y del pezzo;
- sistema di riferimento per le misure successive.

Stato attuale:

- il tool `Localizzazione` appare solo per profili con `imageMode=bw` e `inspectionTypes` contenente `dimensional`;
- dal pannello `Localizzazione` il comando `AOE ricerca` abilita il drag sull'immagine grande;
- al rilascio del mouse la ROI viene salvata in ricetta camera.

## 9. Mappa Rapida Dei File

| File | Responsabilita' |
| --- | --- |
| `src/main.cpp` | Avvio applicazione Qt |
| `src/config/AppConfig.*` | Caricamento configurazione camere |
| `src/config/RecipeManager.*` | Lettura/scrittura dati ricetta |
| `src/config/TranslationManager.*` | Caricamento testi da JSON |
| `src/gui/MainWindow.*` | Finestra principale, menu, layout e coordinamento |
| `src/gui/CameraTileWidget.*` | Miniatura camera |
| `src/gui/ImageViewWidget.*` | Immagine grande, overlay e coordinate mouse/immagine |
| `src/gui/ToolCatalog.*` | Definizione ID tool/azioni |
| `src/gui/ToolPanelWidget.*` | Pannelli placeholder dei tool |
| `src/camera/ICamera.h` | Interfaccia camera |
| `src/camera/FileCamera.*` | Camera simulata da cartella immagini |
| `src/processing/ImageProcessor.*` | Processing OpenCV iniziale |
| `src/utils/Timer.h` | Misura tempi ciclo |
| `config/cameras.json` | Camere, profili, tool GUI |
| `config/app_settings.json` | Impostazioni applicazione, inclusa ricetta attiva |
| `translations/*.json` | Testi GUI traducibili |
| `deploy_release.ps1` | Copia DLL Qt e traduzioni accanto agli exe |
