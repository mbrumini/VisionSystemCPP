# TODO - VisionSystemCPP

## PRIORITA CORRENTE - Debug, ottimizzazione e metrologia (giu 2026)

Vedi anche `PROMEMORIA_DOMANI.md` per la checklist del giorno.

- [ ] Test scalare multicamera: 4-5 → 6-7 → 8-10 → 12-16 camere (tempi, UI, isolamento risultati).
- [ ] Verificare STOP/START produzione: reset pose/geometrie/misure senza tornare al sample.
- [ ] Verificare geometrie in spazio pezzo su rotazione (cerchi, linee; no `anchorInImageSpace` su feature del pezzo).
- [ ] Tarare edge per camera critica: banda, sensibilita', subpixel, filtri; correlare errore pose vs variazione misura su piu' angoli.
- [ ] Profilare colli di bottiglia residui: clone immagini, matToPixmap in griglia, log con dettaglio spento.
- [ ] TestVision (fase successiva): tabella misura x angolo (°), parse `measurements[]` dal JSON simulatore, export report correlazione.
- [ ] NON integrare ancora tool verticali (filettature, ruote dentate, DIN 471-472) finche' la base non e' stabile a 16 camere.

## PRIORITA CORRENTE - Localizzazione AI tramite segmentazione

- [ ] Rimuovere il forcing temporaneo `kForceGuruStartup` quando non servira' piu' avviare sempre il software come Guru.
- [ ] PROSSIMA ATTIVITA OPERATIVA: completare `Localizzazione AI > Labeling` e il primo flusso end-to-end su una ricetta di test, senza mischiare classificazione o segmentazione difetti.
- [x] Definire il flusso: una sola classe `Pezzo`, maschera AI e calcolo OpenCV di centroide, contorno, area e orientamento.
- [x] Separare completamente percorsi e dati da classificazione e segmentazione difetti.
- [x] Creare cartelle dedicate per camera/ricetta: `raw`, `masks`, `labels`, `datasets/<camera>/localization_segmentation` e `models/<camera>/localization_segmentation`.
- [x] Aggiungere `Localizzazione AI` ai tool opzionali configurabili, sempre visibile a Guru.
- [x] Creare pannello dedicato con fasi Acquisizione, Labeling, Training e Inferenza.
- [x] Collegare acquisizione raw singola nella cartella dedicata.
- [x] Implementare primo editor labeling poligonale `Pezzo`, salvando maschera PNG e label YOLO associate senza modificare le immagini raw; ricaricare le label esistenti per modificarle.
- [x] Aggiungere labeling sequenziale del lotto: apertura automatica della prima raw non etichettata, salvataggio e avanzamento automatico, precedente/successiva e conteggio completate/rimanenti.
- [ ] Estendere l'editor labeling con pennello, gomma e annulla/ripristina, riusando lo stesso storage mask/label.
- [ ] Aggiungere acquisizione raw continua con start/stop e intervallo configurabile, riutilizzando il componente della classificazione.
- [ ] Preparare dataset YOLO segmentation con split train/validation/test e immagini negative senza pezzo.
- [x] Implementare il primo training dedicato YOLO segmentation su GPU, con preparazione automatica train/validation ed export ONNX.
- [x] Aggiungere grafico live del training localizzazione con loss segmentazione e mAP50 maschera.
- [x] Aggiungere schermata parametri prima del training localizzazione, coerente con la classificazione: epoch, image size, batch, validation, device e modello base.
- [ ] Aggiungere confronto modello precedente/nuovo e promozione esplicita al training di localizzazione.
- [x] Implementare la prima inferenza GPU dal modello YOLO segmentation addestrato, con worker persistente e selezione automatica dell'ultimo `best.pt`.
- [x] Post-processing OpenCV: selezione maschera, centroide, area, contorno, bounding box, orientamento e controlli di validita'.
- [x] Collegare il risultato alla posa pezzo esistente e mostrare contorno, centro e asse sulla diagnostica.
- [ ] Usare direttamente il modello ONNX per l'inferenza di produzione e misurare il vantaggio rispetto al worker `.pt`.
- [ ] Aggiungere fallback configurabile alla localizzazione tradizionale quando la maschera AI non viene trovata o ha confidenza insufficiente.
- [x] Supportare label YOLO multi-poligono con classe `Pezzo` e più poligoni `Riferimento orientamento`, compatibili con le vecchie label.
- [x] Usare il baricentro complessivo dei riferimenti interni asimmetrici per determinare l'orientamento quando il contorno esterno e' circolare, quadrato o simmetrico.
- [ ] Aggiungere gomma/rimozione selettiva e modifica dei singoli riferimenti già disegnati.

## PRIORITA SUBITO DOPO - Test automatici e stabilita'

- [x] PIPELINE TEST MULTICAMERA - Fase 1: aggiungere `Simulatore` tra le sorgenti configurabili da Sistema per ogni slot `CAM01...CAM16`, insieme a Cartella immagini, USB e Vimba.
- [x] PIPELINE TEST MULTICAMERA - Fase 1: permettere configurazioni miste e indipendenti, per esempio `CAM01=Simulatore`, `CAM02=Simulatore`, `CAM03=Vimba`, mantenendo ricetta, stato e risultati separati per camera.
- [x] PIPELINE TEST MULTICAMERA - Fase 2: definire il protocollo locale bidirezionale tra VisionSystemCPP e il programma esterno TestVision, inizialmente tramite named pipe Windows.
- [x] PIPELINE TEST MULTICAMERA - Fase 2: ogni frame simulato deve contenere almeno versione protocollo, scenarioId, cameraId, slot, canale, frameId, timestamp, formato immagine e payload immagine.
- [x] PIPELINE TEST MULTICAMERA - Fase 2: ogni risultato deve riportare gli stessi scenarioId, cameraId e frameId, oltre a posa, geometrie, misure, giudizi OK/NOK, tempi, warning ed errori.
- [x] PIPELINE TEST MULTICAMERA - Fase 3: implementare `SimulatedCamera` compatibile con `ICamera`, collegabile a qualunque slot e alimentata dai frame ricevuti da TestVision.
- [ ] PIPELINE TEST MULTICAMERA - Fase 3: gestire connessione assente, timeout, frame fuori ordine, duplicati, perdita frame, arresto e riavvio indipendente di una camera simulata.
- [ ] PIPELINE TEST MULTICAMERA - Fase 4: estrarre una pipeline di elaborazione riusabile dalla GUI, chiamata sia dall'HMI sia dalla modalita' simulata, senza automazione dei click e senza duplicare gli algoritmi.
- [x] PIPELINE TEST MULTICAMERA - Fase 4: pubblicare il risultato strutturato al termine della pipeline associandolo sempre allo slot e al frame corretti.
- [x] PIPELINE TEST MULTICAMERA - Fase 5: creare il programma esterno TestVision con doppio ruolo di telecamera simulata e collettore/confrontatore dei risultati.
- [x] PIPELINE TEST MULTICAMERA - Fase 5: TestVision deve caricare una o piu' immagini master, applicare rotazioni, traslazioni, scala e ripetizioni note, calcolare il ground truth trasformato e inviare i frame a VisionSystemCPP.
- [x] PIPELINE TEST MULTICAMERA - Fase 5: usare immagini master asimmetriche quando si verifica l'orientamento, evitando ambiguita' geometriche di 90 o 180 gradi.
- [x] PIPELINE TEST MULTICAMERA - Fase 6: partire con un test verticale su `CAM01` usando una croce asimmetrica, confrontando centro, angolo e stabilita' su piu' ripetizioni.
- [ ] PIPELINE TEST MULTICAMERA - Fase 6: estendere progressivamente a 2, 4 e fino a 16 slot simultanei, verificando isolamento dei risultati, tempi per camera e assenza di blocchi incrociati.
- [x] PIPELINE TEST MULTICAMERA - Fase 7: versionare su Git codice TestVision, scenari JSON, immagini master PNG, ground truth, ricette dedicate, configurazioni simulate e seed casuali per rendere i test replicabili su altri PC.
- [ ] PIPELINE TEST MULTICAMERA - Fase 7: escludere da Git frame temporanei trasformati, workspace di esecuzione, report generati, log e altri artefatti runtime; valutare Git LFS solo per futuri dataset o modelli pesanti.
- [ ] PIPELINE TEST MULTICAMERA - Fase 8: generare report JSON e HTML con riepilogo OK/NOK, errori numerici, stabilita', tempi medi/massimi e immagini diagnostiche dei fallimenti.
- [x] TestVision: salvare il report parziale anche quando il test viene fermato manualmente.
- [x] TestVision: conservare uno storico timestampato dei report mantenendo anche il file `latest` configurato dallo scenario, per confrontare prima/dopo una modifica.
- [x] TestVision: generare immagini AI persistenti direttamente nella cartella raw di localizzazione della ricetta/camera selezionata, con ground truth e manifest JSON versionato, senza cancellare le sessioni precedenti.
- [ ] TestVision: aggiungere tabella misure per angolo (°) e correlazione pose ↔ errore misura (parse `measurements[]` gia' inviato dal simulatore).
- [ ] TestVision: aggiungere confronto automatico tra due report storici con regressioni/miglioramenti su precisione, stabilita' e tempi.
- [x] TestVision: aggiungere `Localizzazione AI YOLO` tra le strategie selezionabili.
- [x] Collegare `aiYolo` alla pipeline simulata end-to-end, mantenendo associazione richiesta/frame e restituendo posa, confidenza e tempi a TestVision.
- [x] TestVision endurance base: caricare una campagna JSON, cambiare automaticamente ricetta e strategia, ed eseguire cicli consecutivi senza intervento manuale.
- [x] TestVision endurance base: produrre un report finale per ricetta/ciclo con problemi di validita', precisione angolare/centro e tempi oltre soglia.
- [ ] TestVision endurance: conservare anche log completi e immagini diagnostiche dei fallimenti separati per ricetta, ciclo e versione software.
- [ ] TestVision endurance: aggiungere regressioni rispetto a una baseline, crash, timeout, uso memoria e stabilita' di lungo periodo.
- [ ] TestVision endurance: supportare ripresa dopo arresto anomalo e non cancellare mai i risultati delle campagne precedenti.
- [ ] Integrare un target test C++ eseguibile con un solo comando prima di avviare o pubblicare il software.
- [ ] Test matematici: intersezioni, centri, distanze, angoli, diametri, conversione pixel/mm e tolleranze OK/NOK.
- [x] Test localizzazione AI base: centroide, area e orientamento calcolati da una maschera nota; salvataggio e ricaricamento mask/label YOLO.
- [ ] Integrare in TestVision scenari end-to-end di localizzazione AI dopo l'attivazione dell'inferenza ONNX.
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

- [ ] Verificare deploy icone su altri PC: quando spariscono, controllare copia plugin Qt SVG `imageformats/qsvg.dll` e `iconengines/qsvgicon.dll` accanto all'exe; validare anche `QT_DIR` e il rilevamento Qt tramite CMakePresets.
- [x] IMPORTANTE: implementare runtime reale multicamera con pipeline/thread separato per ogni camera attiva, fino a 16 camere, e aggiornamento GUI tramite risultati asincroni (base: `CameraAsyncExecutor`, pipeline geometria async).
- [x] Collegare misure future, controlli superficie e AI alla posa corrente invece che alle sole coordinate pixel assolute (in corso: promozione spazio pezzo, reset produzione).
- [ ] Gestire il caso posa non valida: blocco tool dipendenti, risultato NOK o stato di allarme configurabile.
- [ ] Aggiungere sub localizzazione relativa: dopo una localizzazione primaria su shape simmetrico, permettere una seconda ricerca dentro la posa trovata per agganciare dettagli asimmetrici, per esempio un foro non simmetrico, e risolvere ambiguita' di orientamento/posizione.
- [ ] Aggiungere localizzazione pezzi cilindrici/gambi vite: rilevare due bordi paralleli del gambo, calcolare asse centrale, diametro/spessore, angolo, estremita' e riferimenti longitudinali; usare l'asse come posa pezzo per misure successive, controlli filettatura, fase, ammaccature e profilo.

## Geometrie e misure

- [ ] Estendere i detector dedicati alle altre geometrie in ROI relative alla posa pezzo.
- [ ] quando entro nel tool cerchio visualizzo i cerchi di costruzione usati per la localizzazione. non voglio vederli
- [ ] Visualizzare il cerchio in set-up
- [ ] Aggiungere pulsante `Elimina` a tutte le geometrie configurabili: punto, segmento/linea, cerchio, arco, edge e contorno.
- [ ] Per tutte le geometrie, `Nuovo ...` deve creare/selezionare l'elemento e attivare subito il disegno; evitare pulsanti separati tipo `Disegna ...`.
- [x] Salvare configurazione geometrie in ricetta camera.
- [x] Disegnare overlay diagnostici delle geometrie rilevate.
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
