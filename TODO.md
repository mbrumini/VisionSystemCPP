# TODO - VisionSystemCPP

## PRIORITA ALTA - Misure reali e calibrazione

- [ ] Modalita' 1, pezzo campione: per ogni misura salvare valore reale del master, unita', fattore scala e nominale/tolleranze.
- [ ] Modalita' 2, calibrazione camera fissa: associare a ogni camera nel menu Sistema un file calibrazione ottica/camera.
- [ ] Calibrazione con pattern a scacchiera: acquisizione immagini, rilevamento corner, calcolo pixel/mm e futura distorsione.
- [x] Helper base checkerboard: detector corner, stima scala planare, recipe JSON e mappa pixel/mm riusabili.
- [x] Comando Sistema > Calibra checkerboard: calcola scala planare, salva file calibrazione e aggiorna la camera.
- [ ] Calibrazione completa: usare gli stessi frame per stimare matrice camera, distorsione e correzione punti/immagine.
- [ ] Risultati misura: mostrare valore reale in mm/gradi quando disponibile, mantenendo pixel come diagnostica.
- [ ] Tolleranze: nominale, min/max o +/-; giudizio OK/NOK collegato a overview, strip camera e futuro IO.
- [ ] Geometrie costruite prioritarie per misure reali: centro cerchio/arco come punto, distanza tra centri, proiezioni e intersezioni.

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

- [ ] PRIORITA: completare modello dati misura reale prima della GUI avanzata.
- [ ] Implementare acquisizione pattern calibrazione per camera: scatto singolo, lista frame accettati/scartati e salvataggio immagini pattern.
- [ ] Implementare detector pattern calibrazione: checkerboard, dot grid/circle grid, pattern custom.
- [ ] Calcolare modello calibrazione camera: pixel/mm, origine, rotazione e futura mappa distorsione.
- [ ] Salvare/ricaricare calibrazione nella ricetta camera.
- [ ] Convertire `MeasurementResult` pixel in risultati reali: mm/gradi, unita' display, esito.
- [ ] Implementare tolleranze nominale/min/max per misura.
- [ ] Collegare esito misura OK/NOK a overview, camera strip e futuro IO.
- [ ] Aggiungere pannello GUI calibrazione solo dopo aver validato modello dati e formato ricetta.

## Livelli di accesso

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
