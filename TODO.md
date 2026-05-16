# TODO - VisionSystemCPP

## Fatto / Stato attuale

- [x] GUI Qt con griglia 16 miniature, vista camera grande e pannello destro.
- [x] Pannello destro: comandi generali solo in main/griglia; camera, strategie e tool quando una camera e' selezionata.
- [x] Traduzioni JSON italiano/inglese e copia post-build accanto all'eseguibile.
- [x] Ricette come cartelle con creazione, selezione, duplicazione, import ed export.
- [x] Tipologie camera gestite: BN misurazioni, Scala grigi, AI, Scala grigi + AI.
- [x] Localizzazione BW dimensionale con ROI, esclusioni, soglia factor/offset, contorno, centro massa e assi X/Y.
- [x] Localizzazione grigi con strategie `Soglia` e `Bordo`.
- [x] Disegno cerchi con `Centro + raggio` oppure `3 punti`.
- [x] Centro di massa evidenziato con croce diagnostica.
- [x] Strategie localizzazione grigi elencate nel pannello: Soglia, Bordo, Bordo + PCA, Modello, AI YOLO.
- [x] Catalogo strategie GUI separato da `MainWindow`.
- [x] Strategie OpenCV attuali separate in file dedicati.
- [x] Maschere/esclusioni modificabili singolarmente da viewer.
- [x] Fit circolare robusto riusabile con filtro errore regolabile.
- [x] Localizzazione grigi con risultato runtime riusabile: centro, raggio, score, assi e orientamento.
- [x] Strategia Bordo + PCA per sagome con centro e orientamento.
- [x] Strategia Modello con acquisizione da AOE, shape matching e template matching edge-based.

## Localizzazione BW dimensionale

- [x] Mostrare il tool `Localizzazione` solo per camere con `imageMode=bw` e `inspectionTypes` contenente `dimensional`.
- [x] Attivare il disegno AOE/ROI solo dal pannello `Localizzazione`.
- [x] Disegnare il rettangolo con mouse drag sull'immagine grande.
- [x] Convertire coordinate widget in coordinate immagine originale.
- [x] Salvare la ROI nella ricetta della camera corrispondente.
- [x] Implementare test localizzazione: sfondo chiaro, pezzo nero, soglia da campionamento angoli.
- [x] Aggiungere parametri soglia `factor`/`offset` nella ricetta.
- [x] Disegnare contorno pezzo, centro di massa e assi X/Y orientati.
- [x] Salvare il risultato localizzazione come struttura dati riusabile.
- [x] Aggiungere rettangoli rossi di esclusione nella ROI per ignorare disturbi.
- [x] Aggiungere modifica/cancellazione singola delle esclusioni.

## Localizzazione superficie grayscale

- [x] Separare i comandi superficiali dalla localizzazione dimensionale.
- [x] Aggiungere localizzazione superficie con AOE, esclusioni e soglia per camere grayscale/surface.
- [x] Calcolare contorno principale e centro di massa anche su grayscale.
- [x] Aggiungere prima strategia `two_circles_axis` con due cerchi, soglie e polarita'.
- [x] Aggiungere selezione metodo localizzazione grayscale: soglia oppure bordo.
- [x] Aggiungere cerchio bordo a 3 click e fascia interna/esterna per metodo edge.
- [x] Preparare pannello strategie nel lato destro senza log eventi e senza comandi Start/Stop.
- [x] Separare il catalogo strategie in file dedicati (`SurfaceLocalizationStrategies.*`).
- [x] Separare le strategie OpenCV attuali in file dedicati (`SurfaceThresholdStrategy`, `SurfaceEdgeStrategy`, `SurfaceTwoCirclesStrategy`).
- [x] Salvare il risultato runtime della localizzazione grigi per camera: centro pezzo, raggio, score e qualita' fit.
- [ ] Usare sempre il centro localizzato del pezzo corrente come origine dinamica per le misure successive.
- [ ] Aggiungere maniglie grafiche per regolare direttamente la fascia edge.
- [ ] Implementare controlli dedicati strategia `Soglia`: soglia, polarita', area min/max, maschere.
- [ ] Implementare controlli dedicati strategia `Bordo`: guida, fascia, sensibilita', area min/max, maschere.
- [x] Implementare strategia `Bordo + PCA`: punti bordo, min punti, centro massa e orientamento.
- [x] Aggiungere funzioni processing per `Modello`: shape matching classico e template/model matching edge-based con sweep angolare.
- [x] Implementare GUI/ricetta base per `Modello`: acquisizione modello, salvataggio template/contorno, prove shape e template.
- [x] Aggiungere controlli avanzati `Modello`: range angolo, score minimo, distanza shape massima, sensibilita' bordo.
- [ ] Migliorare diagnostica `Modello`: viste separate template, edge map e contorno acquisito.
- [ ] Implementare strategia `AI YOLO`: modello ONNX, classe target, confidenza, soglia maschera, centro/orientamento da mask.
- [ ] Aggiungere editor GUI delle feature strategia.

## Ricette

- [x] Creare struttura iniziale `recipes/default/`.
- [x] Salvare tool camera in `recipes/default/cameras/CAMxx.json`.
- [x] Aggiungere selezione ricetta dal menu superiore.
- [x] Aggiungere creazione/duplicazione/import/export ricetta come cartella.
- [x] Salvare ricetta attiva in `config/app_settings.json`.
- [ ] Aggiungere export/import in formato archivio zip.
