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
9. Stato fatto
10. Mappa rapida dei file

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
- parametri localizzazione `threshold.factor` e `threshold.offset`, usati per calcolare:
  `soglia = sfondo * factor + offset`.
- rettangoli `exclusionRects` in coordinate immagine originale per mascherare aree
  che disturbano la localizzazione.

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
- tool dinamici della camera.

Layout pannello destro:

- in vista griglia/main mostra i comandi generali: `Start`, `Stop`, `Reset errori`,
  `Reload config`, `Esci`;
- quando una camera e' selezionata mostra solo camera selezionata, pulsante
  `Vista griglia`, strategie disponibili e tool camera;
- `Start`/`Stop` restano anche nel menu `Sistema`;
- il box `Log eventi` non e' piu' visibile nel pannello destro. Le chiamate
  `appendLog()` restano nel codice come diagnostica interna e sono no-op se il
  widget log non esiste.

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
- rettangoli rossi di esclusione;
- cerchi su immagine;
- acquisizione cerchio a 3 punti;
- callback quando ROI, esclusioni o cerchi cambiano.

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

Per camere grayscale con controllo superficie, la localizzazione dimensionale non viene
usata. Il tool separato `Localizzazione grigi` gestisce invece:

- AOE superficie;
- rettangoli di esclusione;
- test soglia grayscale;
- contorno principale e centro di massa.

Le strategie di localizzazione grigi sono elencate nel pannello destro quando una
camera grayscale e' selezionata:

- `Soglia`: implementata;
- `Bordo`: implementata;
- `Bordo + PCA`: pianificata;
- `Modello`: pianificata;
- `AI YOLO`: pianificata.

Il catalogo GUI delle strategie e' separato in:

- `src/gui/SurfaceLocalizationStrategies.h`
- `src/gui/SurfaceLocalizationStrategies.cpp`

Le strategie OpenCV attuali sono separate in:

- `SurfaceThresholdStrategy.*`: soglia in ROI e corona;
- `SurfaceEdgeStrategy.*`: bordo Canny in corona/fascia;
- `SurfaceTwoCirclesStrategy.*`: strategia due feature circolari e assi;
- `SurfaceProcessingUtils.h`: helper comuni per rettangoli, marker centro massa,
  ordinamento blob e disegno diagnostico.

`SurfaceDefectProcessor.*` resta come facciata compatibile con il resto della GUI:
riceve le chiamate esistenti e delega alle strategie specifiche.

Stato attuale:

- il tool `Localizzazione` appare solo per profili con `imageMode=bw` e `inspectionTypes` contenente `dimensional`;
- dal pannello `Localizzazione` il comando `AOE ricerca` abilita il drag sull'immagine grande;
- al rilascio del mouse la ROI viene salvata in ricetta camera.
- dal pannello `Localizzazione` il comando `Aggiungi esclusione` abilita il drag di
  rettangoli rossi, salvati in `exclusionRects`.
- `Cancella esclusioni` svuota tutte le esclusioni della camera selezionata.
- il comando `Test localizzazione` usa OpenCV per:
  - campionare lo sfondo negli angoli della ROI;
  - calcolare soglia da `threshold.factor` e `threshold.offset` salvati in ricetta;
  - azzerare nella maschera binaria le aree definite da `exclusionRects`;
  - trovare il contorno scuro principale;
  - calcolare centro di massa, bounding box, contorno e orientamento;
  - salvare il risultato in una struttura `LocalizationResult`;
  - mostrare immagine diagnostica con ROI, contorno, bounding box, centro e assi X/Y orientati.

Per profili `grayscale` con `inspectionTypes` contenente `surface`, il tool
`Localizzazione grigi` espone comandi separati:

- `AOE superficie`: salva `tools.surfaceLocalization.searchRoi`;
- `Aggiungi esclusione`: salva rettangoli in `tools.surfaceLocalization.exclusionRects`;
- `Cancella esclusioni`: svuota le esclusioni superficiali;
- metodo `threshold`: usa una corona esterno/interno a centro comune e
  `tools.surfaceLocalization.threshold.value` per creare una maschera grayscale;
- metodo `edge`: usa una guida bordo ricavata da 3 click e una fascia
  `tools.surfaceLocalization.edge.band.inner/outer` per cercare contorni Canny
  quando pezzo e sfondo sono troppo simili per la soglia.

Entrambi i metodi applicano le esclusioni rosse prima di cercare il contorno
principale, bounding box e centro di massa. Il centro di massa e' disegnato con
croce gialla bordata di nero piu' punto rosso.

Disegno cerchi:

- `Centro + raggio`: l'operatore clicca/trascina per impostare un cerchio;
- `3 punti`: l'operatore clicca tre punti, il sistema calcola la guida e crea
  automaticamente la corona interna/esterna usando la fascia configurata;
- in `Bordo`, la guida non e' il bordo trovato: serve solo a definire dove cercare
  il bordo reale.

La localizzazione superficie supporta anche la prima strategia configurabile:
`two_circles_axis`.

Esempio ricetta:

```json
"surfaceLocalization": {
  "method": "grayscale_threshold",
  "strategy": {
    "name": "two_circles_axis",
    "origin": "midpoint",
    "xAxis": {
      "from": "circle_a",
      "to": "circle_b"
    },
    "features": [
      {
        "id": "circle_a",
        "type": "circle",
        "polarity": "NB",
        "searchRoi": { "x": 1000, "y": 360, "width": 220, "height": 220 },
        "threshold": { "min": 0, "max": 80 },
        "expectedRadius": { "min": 20, "max": 80 }
      },
      {
        "id": "circle_b",
        "type": "circle",
        "polarity": "NB",
        "searchRoi": { "x": 1330, "y": 360, "width": 220, "height": 220 },
        "threshold": { "min": 0, "max": 80 },
        "expectedRadius": { "min": 20, "max": 80 }
      }
    ]
  }
}
```

La strategia cerca il contorno principale dentro ogni ROI feature, valida il raggio
atteso quando configurato, calcola i centri di massa dei due cerchi e costruisce
l'asse X dal feature `from` al feature `to`. L'origine puo' essere `midpoint` o
l'ID di una feature.

## 9. Stato Fatto

Stato funzionale consolidato:

- GUI Qt con 16 miniature e vista camera grande;
- menu superiore con ricette, lingua e sistema;
- traduzioni JSON copiate accanto all'eseguibile in post-build;
- ricette come cartelle, con selezione/creazione/duplicazione/import/export;
- camere classificate come BN misurazioni, Scala grigi, AI, Scala grigi + AI;
- localizzazione BW dimensionale con ROI, soglia factor/offset, esclusioni,
  contorno, centro massa, orientamento e assi X/Y;
- localizzazione grigi con strategie `Soglia` e `Bordo`;
- maschere rosse che escludono aree disturbanti;
- disegno cerchi con `Centro + raggio` o `3 punti`;
- cancellazione maschere/cerchi per localizzazione grigi;
- centro di massa evidenziato con croce;
- pannello destro pulito: comandi generali solo in main/griglia, strategie/tool
  quando una camera e' selezionata;
- catalogo strategie GUI e strategie OpenCV separati in file dedicati.

## 10. Mappa Rapida Dei File

| File | Responsabilita' |
| --- | --- |
| `src/main.cpp` | Avvio applicazione Qt |
| `src/config/AppConfig.*` | Caricamento configurazione camere |
| `src/config/RecipeManager.*` | Lettura/scrittura dati ricetta |
| `src/config/TranslationManager.*` | Caricamento testi da JSON |
| `src/gui/MainWindow.*` | Finestra principale, menu, layout e coordinamento |
| `src/gui/CameraTileWidget.*` | Miniatura camera |
| `src/gui/ImageViewWidget.*` | Immagine grande, overlay e coordinate mouse/immagine |
| `src/gui/SurfaceLocalizationStrategies.*` | Catalogo strategie localizzazione grigi |
| `src/gui/ToolCatalog.*` | Definizione ID tool/azioni |
| `src/gui/ToolPanelWidget.*` | Pannelli placeholder dei tool |
| `src/camera/ICamera.h` | Interfaccia camera |
| `src/camera/FileCamera.*` | Camera simulata da cartella immagini |
| `src/processing/ImageProcessor.*` | Processing OpenCV iniziale |
| `src/processing/LocalizationProcessor.*` | Localizzazione BW: soglia, contorno e centro di massa |
| `src/processing/SurfaceDefectProcessor.*` | Facciata compatibile per strategie superficie |
| `src/processing/SurfaceThresholdStrategy.*` | Strategia grigi a soglia |
| `src/processing/SurfaceEdgeStrategy.*` | Strategia grigi a bordo/Canny |
| `src/processing/SurfaceTwoCirclesStrategy.*` | Strategia due cerchi/assi |
| `src/processing/SurfaceProcessingUtils.h` | Helper comuni strategie superficie |
| `src/utils/Timer.h` | Misura tempi ciclo |
| `config/cameras.json` | Camere, profili, tool GUI |
| `config/app_settings.json` | Impostazioni applicazione, inclusa ricetta attiva |
| `translations/*.json` | Testi GUI traducibili |
| `deploy_release.ps1` | Copia DLL Qt e traduzioni accanto agli exe |
