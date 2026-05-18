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
## Gui 
- [x] lasciare solo Cam X, eliminare le repliche X e poi X di nuovo dalle miniature
- [x] togliere il pulsante Esci dalla gui lasciarlo solo nel menù a tendina
- [x] aggiungere 1 livello per la localizzazione, primo pulsante solo localizzazione che apre una finestra con la lista delle possibilità
- [x] comandi linea, fascia linea e sensibilità bordo su una riga, pulizia edge e filtro statistico sulla seconda riga anche direzione linea e transizione bordo su una linea
- [x] quando premo Nuova linea il sistema deve prepararsi per fare una nuova linea, il pulsante disegna linea 2 punti è inutile, automatizzare ed eliminare il pulsante inutile


## Runtime camera e origine pezzo

- [x] Aggiungere struttura runtime per camera con sorgente, frame corrente, stato, risultato tool e posa pezzo corrente.
- [ ] IMPORTANTE: implementare runtime reale multicamera con pipeline/thread separato per ogni camera attiva, fino a 16 camere, e aggiornamento GUI tramite risultati asincroni.
- [x] Definire una struttura comune `PiecePose`/`PartPose` valida per BW e grayscale: origine, angolo, assi, score, metodo e validita'.
- [x] Aggiornare la posa pezzo a ogni localizzazione riuscita, indipendentemente dalla strategia usata.
- [x] Aggiungere funzioni di conversione coordinate immagine <-> coordinate pezzo per riusare l'origine dinamica nei tool successivi.
- [ ] Collegare misure future, controlli superficie e AI alla posa corrente invece che alle sole coordinate pixel assolute.
- [ ] Gestire il caso posa non valida: blocco tool dipendenti, risultato NOK o stato di allarme configurabile.

## Geometrie e misure

- [x] Separare il flusso in due passaggi: prima geometrie rilevate, poi misure calcolate sulle geometrie.
- [x] Aggiungere modelli geometrici separati per tipo: punto, linea, cerchio, arco, edge e contorno.
- [x] Aggiungere contenitore runtime `GeometrySet` per camera.
- [x] Aggiungere primo detector geometrico riusabile: cerchio da soglia/contorno con fit robusto.
- [x] Aggiungere primo flusso operativo `Geometrie -> Linea`: pannello parametri, linea guida a 2 punti, fascia regolabile relativa alla posa pezzo, detector edge, diagnostica e salvataggio runtime.
- [x] Separare la gestione click a 2 punti della linea da `ImageViewWidget` usando `LineGeometryEditor`.
- [x] Creare editor dedicato `LineGeometryEditor`: due punti, fascia e configurazione linea.
- [x] Creare controller mouse dedicato `LineGeometryMouseController` per click e anteprima linea.
- [x] Creare overlay geometrie riusabile per punti, linee e fasce orientate.
- [x] Aggiungere refresh live della linea quando cambiano fascia e sensibilita' edge.
- [x] Correggere bug linea: dopo il secondo punto restano visibili punto 1, punto 2, linea guida e fascia.
- [x] Aggiungere scansione linea configurabile: direzione normale, transizione BN/NB e scelta edge primo/ultimo/migliore.
- [x] Separare filtro punti edge in `EdgePointFilters` e aggiungere pulizia edge per derivata sui punti linea.
- [x] Aggiungere filtro statistico riusabile sui punti edge basato su deviazione dalla mediana normale.
- [x] Aggiungere gestione multi-linea nel pannello `Linea`: lista linee, `Nuova linea`, linea attiva modificabile e salvataggio ricetta.
- [x] Mostrare tutte le linee configurate in overlay e scansionarle tutte nel runtime/setup, non solo l'ultima selezionata.
- [x] Aggiungere subpixel edge per linee su camere BN e nascondere l'opzione sulle camere non BN.
- [x] Nascondere le maniglie della linea nel setup/runtime: restano visibili solo nel tool di modifica.
- [x] al passaggio da set-up a qualsiasi altra finestra ricaricare immagine di riferimento
- [X] Aggiungere gestione multi-Punto nel pannello `Punto`: lista linee, `Nuovo punto`, punto attivo modificabile e salvataggio ricetta.
- [x] Visualizzare sempre il centro di massa durante il set-up
- [x] in set up, possibilità di portare il timer a 0 (ora minimo 50) ed inserire un timer per capire il tempo di scansione
- [x] Separare anche la gestione maniglie/fascia in un controller/editor riusabile per tutti gli editor geometrici.
- [ ] Estendere i detector dedicati alle altre geometrie in ROI relative alla posa pezzo.
- [x] Aggiungere detector dedicato `Cerchio`: guida a 3 punti, fascia edge interna/esterna, transizioni, filtri, subpixel BN e fit robusto.
- [ ] Aggiungere pulsante `Elimina` a tutte le geometrie configurabili: punto, segmento/linea, cerchio, arco, edge e contorno.
- [ ] Per tutte le geometrie, `Nuovo ...` deve creare/selezionare l'elemento e attivare subito il disegno; evitare pulsanti separati tipo `Disegna ...`.
- [ ] Salvare configurazione geometrie in ricetta camera.
- [ ] Disegnare overlay diagnostici delle geometrie rilevate.
- [ ] Implementare misure che usano geometrie gia' rilevate, per esempio distanza punto-linea.
- [ ] Implementare tolleranze/criteri su misure e relazioni tra geometrie, per esempio concentricita' tra due cerchi.


## Ricette

- [x] Creare struttura iniziale `recipes/default/`.
- [x] Salvare tool camera in `recipes/default/cameras/CAMxx.json`.
- [x] Aggiungere selezione ricetta dal menu superiore.
- [x] Aggiungere creazione/duplicazione/import/export ricetta come cartella.
- [x] Salvare ricetta attiva in `config/app_settings.json`.
- [x] Aggiungere per ogni camera della ricetta una cartella `images/CAMxx/sample/` con l'immagine campione usata per programmare i tool.
- [x] Aggiungere per ogni camera della ricetta una cartella `images/CAMxx/test/` con immagini di test usate per simulare e validare la ricetta.
- [ ] Separare chiaramente immagine campione e immagini test: i tool si impostano sul campione, Start/Frame successivo scorre i test.
- [ ] Aggiungere export/import in formato archivio zip.

## Simulazione acquisizione

- [ ] Aggiungere menu alto `Telecamere` con la stessa lista delle miniature.
- [ ] Da `Telecamere -> CAMxx`, aprire una finestra setup per associare una cartella immagini alla camera.
- [ ] Salvare la cartella immagini in `config/cameras.json` come sorgente camera simulata, non nella ricetta prodotto.
- [ ] Aggiungere voce `Setup` nel pannello destro della camera selezionata.
- [ ] Nel pannello `Setup`, mostrare sorgente, stato, frame corrente, Start/Stop, step manuale, loop e intervallo frame.
- [ ] Aggiungere Start/Stop simulazione per scorrere le immagini come frame in arrivo.
- [ ] Aggiornare preview camera e vista grande a ogni frame simulato.
- [ ] Eseguire la localizzazione attiva a ogni frame e aggiornare il risultato runtime del pezzo corrente.
- [ ] Eseguire i tool programmati per la camera dopo la localizzazione: diametro, superficie, AI e altri controlli futuri.
- [ ] Progettare la simulazione in modo che `FileCamera` sia sostituibile in seguito con `VimbaCamera` senza cambiare la pipeline tool.

## Divisione compiti

- [ ] Controllare che non ci siano funzioni diverse annidate nello stesso modulo, tenere tutte le funzioni separate in modo da poterle riutilizzare e non avere blocchi di codice infiniti
