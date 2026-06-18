# TODO - VisionSystemCPP

## PRIORITA CORRENTE - Localizzazione AI tramite segmentazione

- [x] Definire il flusso: una sola classe `Pezzo`, maschera AI e calcolo OpenCV di centroide, contorno, area e orientamento.
- [x] Separare completamente percorsi e dati da classificazione e segmentazione difetti.
- [x] Creare cartelle dedicate per camera/ricetta: `raw`, `masks`, `labels`, `datasets/<camera>/localization_segmentation` e `models/<camera>/localization_segmentation`.
- [x] Aggiungere `Localizzazione AI` ai tool opzionali configurabili, sempre visibile a Guru.
- [x] Creare pannello dedicato con fasi Acquisizione, Labeling, Training e Inferenza.
- [x] Collegare acquisizione raw singola nella cartella dedicata.
- [ ] PROSSIMO PASSO: implementare editor labeling maschera `Pezzo`, salvando maschera e label associati senza modificare le immagini raw.
- [ ] Aggiungere acquisizione raw continua con start/stop e intervallo configurabile, riutilizzando il componente della classificazione.
- [ ] Preparare dataset YOLO segmentation con split train/validation/test e immagini negative senza pezzo.
- [ ] Implementare training dedicato, grafico live, confronto modello precedente/nuovo e promozione esplicita.
- [ ] Esportare e usare il modello ONNX per inferenza veloce.
- [ ] Post-processing OpenCV: selezione maschera, centroide, area, contorno, bounding box, orientamento e controlli di validita'.
- [ ] Collegare il risultato alla posa pezzo esistente e aggiungere fallback alla localizzazione tradizionale.

## PRIORITA SUBITO DOPO - Test automatici e stabilita'

- [ ] Integrare un target test C++ eseguibile con un solo comando prima di avviare o pubblicare il software.
- [ ] Test matematici: intersezioni, centri, distanze, angoli, diametri, conversione pixel/mm e tolleranze OK/NOK.
- [ ] Test localizzazione AI: centroide, area e orientamento calcolati da maschere note.
- [ ] Test detector su immagini fisse con risultati attesi e tolleranze numeriche: punto, linea, cerchio, arco e localizzazione pezzo.
- [ ] Test negativi: contorno/modello/pezzo assente deve produrre uno stato coerente senza risultati intermittenti.
- [ ] Test di stabilita': elaborare piu' volte lo stesso frame e verificare che geometrie, posa, overlay e misure non appaiano e scompaiano.
- [ ] Test ricette: caricamento, salvataggio e ricostruzione geometrie/misure usando esclusivamente ricette temporanee di test.
- [ ] Test accessi e feature: tool opzionali nascosti agli utenti quando disabilitati, sempre visibili a Guru.
- [ ] Garantire che i test non modifichino mai configurazioni, calibrazioni, immagini o ricette operative reali.
- [ ] Eseguire automaticamente la suite prima di ogni push stabile.
- [ ] Creare un programma esterno `VisionSystemTestRunner`, separato dall'HMI operativa, per test end-to-end con log e report.
- [ ] Aggiungere a VisionSystemCPP una modalita' test non interattiva, per esempio `--test <scenario.json> --output <result.json>`, senza automazione fragile dei click.
- [ ] Definire scenari JSON con ricetta di test, camera, immagine/frame, tool da eseguire, risultati attesi e tolleranze numeriche.
- [ ] Fare usare al runner esclusivamente workspace temporanei, ricette dedicate e copie delle immagini di test.
- [ ] Permettere al runner di avviare VisionSystemCPP, inviare lo scenario, imporre un timeout, acquisire exit code, stdout/stderr e log dettagliato.
- [ ] Restituire risultati strutturati JSON: posa, geometrie, misure, giudizi OK/NOK, tempi, warning ed errori.
- [ ] Confrontare automaticamente risultati reali e attesi, inclusi test negativi e ripetizioni dello stesso frame.
- [ ] Generare report leggibili HTML e JSON con riepilogo OK/NOK, differenze numeriche, immagini diagnostiche e riferimenti ai log.
- [ ] Aggiungere una suite di regressione stabile da eseguire prima dei push e delle release.
- [ ] Valutare successivamente named pipe o socket locale per comandare e osservare anche un'istanza HMI aperta; partire dalla modalita' CLI, piu' semplice e affidabile.

## PRIORITA ALTA - Misure reali e calibrazione

- [x] Modalita' 1, pezzo campione: per ogni misura salvare valore reale del master, unita' e fattore scala base.
- [x] Modalita' 2, calibrazione camera fissa: associare a ogni camera nel menu Sistema un file calibrazione ottica/camera.
- [x] Calibrazione checkerboard base: pagina live, blocco frame, rilevamento corner, calcolo pixel/mm e salvataggio file camera.
- [x] Helper base checkerboard: detector corner, stima scala planare, recipe JSON e mappa pixel/mm riusabili.
- [x] Comando Sistema > Calibra checkerboard: calcola scala planare, salva file calibrazione e aggiorna la camera.
- [ ] Validare end-to-end calibrazione base con checkerboard reale: acquisizione live, file `calibrations/CAMxx_checkerboard.json`, associazione camera e misure in mm senza pezzo campione.
- [ ] Risultati misura: indicare chiaramente se il valore reale arriva da pezzo campione o da calibrazione camera.
- [ ] Calibrazione completa: usare gli stessi frame per stimare matrice camera, distorsione e correzione punti/immagine.
- [x] Risultati misura: mostrare valore reale in mm/gradi quando disponibile, mantenendo pixel come diagnostica.
- [ ] Tolleranze: nominale, min/max o +/-; giudizio OK/NOK collegato a overview, strip camera e futuro IO.
- [x] Geometrie costruite prioritarie per misure reali: centro cerchio/arco come punto, distanza tra centri, proiezioni e intersezioni.

## Localizzazione superficie grayscale

- [ ] Usare sempre il centro localizzato del pezzo corrente come origine dinamica per le misure successive.
- [ ] Aggiungere maniglie grafiche per regolare direttamente la fascia edge.
- [ ] Implementare controlli dedicati strategia `Soglia`: soglia, polarita', area min/max, maschere.
- [ ] Implementare controlli dedicati strategia `Bordo`: guida, fascia, sensibilita', area min/max, maschere.
- [ ] Migliorare diagnostica `Modello`: viste separate template, edge map e contorno acquisito.
- [ ] Implementare strategia `AI YOLO`: modello ONNX, classe target, confidenza, soglia maschera, centro/orientamento da mask.
- [ ] Aggiungere editor GUI delle feature strategia.

## AI

- [ ] Help/Ollama: aggiornare la lista dei file help in base alle modifiche operative su classificazione, segmentazione e installer.
- [ ] Classificazione: visualizzare riepilogo dataset per classe e warning sbilanciamento direttamente nel pannello.
- [ ] Classificazione: salvare nella ricetta il modello attivo (`best.pt`/`best.onnx`), soglia confidenza e mappa classi.
- [ ] Classificazione: tenere un solo modello attivo/stabile per camera, evitando una lista ingestibile di modelli. Il training nuovo deve finire in un'area temporanea; al termine mostrare grafico e confronto vecchio vs nuovo (accuracy/loss, confusion matrix se disponibile, esempi inferenza su validation set), poi scegliere se promuovere il nuovo modello sovrascrivendo quello attivo oppure scartarlo e mantenere il precedente.
- [ ] Classificazione: aggiungere test inferenza su frame corrente con risultato classe/confidenza.
- [ ] Segmentazione: completare labeling a pennello, dataset YOLO segmentation e training dedicato. Base feature dinamiche e cartelle immagini/maschere/label gia' predisposta.
- [ ] Segmentazione: usare lo stesso schema UI della classificazione: pulsante `Training` apre parametri a destra, grafico live a sinistra, training temporaneo, confronto vecchio/nuovo e promozione esplicita di un solo modello attivo.
- [ ] Segmentazione: usare la maschera per centro, orientamento, area e controlli dimensionali/forma.
- [ ] Anomaly detection: definire raccolta immagini buone, training e soglia anomalia.
- [ ] Anomaly detection: usare lo stesso schema UI della classificazione: parametri training a destra, grafico live a sinistra, confronto modello precedente/nuovo e promozione esplicita di un solo modello attivo.
- [ ] Anomaly detection: mostrare heatmap/overlay difetto e score OK/NOK.

## Runtime camera e origine pezzo

- [ ] Verificare deploy icone su altri PC: quando spariscono, controllare copia plugin Qt SVG `imageformats/qsvg.dll` e `iconengines/qsvgicon.dll` accanto all'exe; validare anche CMakePresets/CMAKE_PREFIX_PATH se Qt non e' in `D:/Qt/6.10.3/msvc2022_64`.
- [ ] IMPORTANTE: implementare runtime reale multicamera con pipeline/thread separato per ogni camera attiva, fino a 16 camere, e aggiornamento GUI tramite risultati asincroni.
- [ ] Collegare misure future, controlli superficie e AI alla posa corrente invece che alle sole coordinate pixel assolute.
- [ ] Gestire il caso posa non valida: blocco tool dipendenti, risultato NOK o stato di allarme configurabile.
- [ ] Aggiungere sub localizzazione relativa: dopo una localizzazione primaria su shape simmetrico, permettere una seconda ricerca dentro la posa trovata per agganciare dettagli asimmetrici, per esempio un foro non simmetrico, e risolvere ambiguita' di orientamento/posizione.
- [ ] Aggiungere localizzazione pezzi cilindrici/gambi vite: rilevare due bordi paralleli del gambo, calcolare asse centrale, diametro/spessore, angolo, estremita' e riferimenti longitudinali; usare l'asse come posa pezzo per misure successive, controlli filettatura, fase, ammaccature e profilo.

## Geometrie e misure

- [ ] Estendere i detector dedicati alle altre geometrie in ROI relative alla posa pezzo.
- [ ] quando entro nel tool cerchio visualizzo i cerchi di costruzione usati per la localizzazione. non voglio vederli
- [ ] Visualizzare il cerchio in set-up
- [ ] Aggiungere pulsante `Elimina` a tutte le geometrie configurabili: punto, segmento/linea, cerchio, arco, edge e contorno.
- [ ] Per tutte le geometrie, `Nuovo ...` deve creare/selezionare l'elemento e attivare subito il disegno; evitare pulsanti separati tipo `Disegna ...`.
- [ ] Salvare configurazione geometrie in ricetta camera.
- [ ] Disegnare overlay diagnostici delle geometrie rilevate.
- [ ] Provare in GUI il flusso completo `Misure` con immagini reali.
- [ ] Implementare tolleranze/criteri su misure e relazioni tra geometrie, per esempio concentricita' tra due cerchi.
- [ ] IMPORTANTE: aggiungere tool `Filettature` con rilevamento e misure dedicate: diametro interno/esterno o maggiore/minore, passo filetto, cresta e fondo filetto, angolo profilo, profondita'/altezza filetto, numero creste/gole, presenza filetto, posizione rispetto a riferimenti, tolleranze OK/NOK e diagnostica overlay. Includere controlli su fase/smussi di ingresso, ammaccature o schiacciamenti del filetto/bordo, bave, profilo completo del filetto, continuita' del profilo e confronto con profilo campione/master. Prevedere uso su profilo BN/silhouette e, dove possibile, su superficie con illuminazione adeguata.

## Misure reali e calibrazione

- [x] PRIORITA: completare modello dati misura reale base prima della GUI avanzata.
- [ ] Implementare acquisizione pattern calibrazione per camera: scatto singolo, lista frame accettati/scartati e salvataggio immagini pattern.
- [x] Implementare detector pattern calibrazione base: checkerboard e predisposizione dot/circle grid.
- [x] Calcolare modello calibrazione camera base: pixel/mm, origine e rotazione.
- [x] Salvare/ricaricare calibrazione nella configurazione camera.
- [x] Convertire `MeasurementResult` pixel in risultati reali: mm/gradi e unita' display.
- [ ] Implementare tolleranze nominale/min/max per misura.
- [ ] Collegare esito misura OK/NOK a overview, camera strip e futuro IO.
- [x] Aggiungere pannello GUI calibrazione base con live, blocco frame e invio calibrazione.

## Livelli di accesso

- [x] Predisporre menu Accesso con login tramite password e configuratore accessi.
- [x] Predisporre utenti con alias, ruolo, stato, password personale, diritti personalizzati e note.
- [x] Predisporre matrice ruoli/diritti per Viewer, Operatore, Supervisore e Amministratore; Guru resta nascosto e riservato.
- [x] Aggiungere opzione riservata Guru `Avvia sempre come Guru`, persistente sul PC.
- [x] Aggiungere configuratore tool opzionali AI; i tool fondamentali restano sempre disponibili e Guru vede sempre tutto.
- [ ] Definire matrice permessi definitiva per Viewer/Operatore/Supervisore/Admin/Guru.
- [ ] Aggiungere logout o selezione livello con PIN/password configurabili.
- [ ] Persistenza utenti/livelli configurabili da exe: Operatore, Supervisore, Admin.
- [ ] Escludere sempre la backdoor dai file utenti/livelli configurabili.
- [ ] Applicare permessi ai comandi HMI: start/stop, setup, geometrie, misure, calibrazione, ricette, sorgenti camera, sistema.
- [ ] Rendere visibile in toolbar il livello utente corrente.
- [ ] Loggare cambi livello e tentativi di accesso negati.

## Ricette

- [ ] Separare chiaramente immagine campione e immagini test: i tool si impostano sul campione, Start/Frame successivo scorre i test.
- [ ] Aggiungere export/import in formato archivio zip.

## Divisione compiti

- [ ] Controllare che non ci siano funzioni diverse annidate nello stesso modulo, tenere tutte le funzioni separate in modo da poterle riutilizzare e non avere blocchi di codice infiniti



## Consigli

Suggerimenti generali brevi e pratici:

- Eseguire elaborazione pesante, detector, trainer e processor fuori dal thread UI con QtConcurrent/QThreadPool per evitare freeze della UI.
- Ridurre duplicazioni: molte funzioni ripetono il pattern "carica immagine -> valida -> crea config -> esegui detector -> aggiornamento UI"; estrarre helper comuni come `runAsyncDetector` o `showDiagnosticImageAndRoi`.
- Evitare closure che catturano copie pesanti: catturare `const CameraConfig&` quando possibile.
- Centralizzare la costruzione dei log con helper tipo `logInfo()`/`logError()`.
- Spostare lo stylesheet in risorsa qss o file esterno per manutenibilita'.
- Considerare piccoli helper const/ref per `projectPath` e `resolveProjectPath`; gia' OK, ma attenzione a macro compile-time.
- Esempio concreto: rendere `testGeometryLine` asincrono con QtConcurrent, evitando il blocco della UI mentre `EdgeLineDetector::detect(...)` esegue e aggiornando la UI a termine con `QFutureWatcher`.
