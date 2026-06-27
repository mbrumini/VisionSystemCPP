# TODO - VisionSystemCPP

Unica lista operativa del progetto. Quando una voce cambia stato, aggiornarla qui
invece di creare promemoria separati.

Legenda:

- `[ ]` da fare
- `[~]` in corso / da validare
- `[x]` fatto

English note: this file is the single project backlog. The Italian section below
is the authoritative detailed list. A concise English summary is available near
the end of the file for collaborators.

## Priorita immediata

- [ ] Test reale su macchina: validare camera USB/Vimba, luce, vibrazioni, pezzi veri e tempi ciclo.
- [ ] Pulizia prima collaborazione: decidere quali ricette demo restano versionate e quali dataset reali restano solo locali.
- [ ] Stabilizzare tool filetto: fase vicina a zero su SVG/campione in fase, poi creare campione fuori fase 5-6 gradi.
- [ ] Sistemare misura `Diametro punta` nella ricetta `stress_test`: oggi nei report multicamera risulta non valida.
- [ ] Configurare tolleranze anche per diametro medio filetto (`pitchDiameter`).
- [ ] Rinominare misure provvisorie `measurement_7...measurement_10` nella ricetta `stress_test` e portarle in mm se servono davvero.
- [ ] Validare START/STOP produzione con 2, 4, 8 camere: reset coerente di pose, geometrie e misure.
- [ ] Eseguire una campagna TestVision con CAM01/CAM02 in raccolta dati e confrontare report storici.
- [ ] Ridurre tempi build: verificare perche' MSBuild ricompila molte sorgenti in una sola invocazione `cl.exe`.

## Fatto recente

- [x] Aggiunta ricetta `stress_test` con geometrie, misure, filetto e campione dedicato.
- [x] Duplicata ricetta `stress_test` per CAM02-CAM08/CAM09 come base multicamera.
- [x] Aggiunto tool costruito `midline_between_lines` per creare una linea media tra due linee.
- [x] Evidenziate in overlay le geometrie selezionate durante creazione misure/geometrie costruite.
- [x] Aggiunte misure filetto: diametro esterno, interno, passo, fase, diametro medio.
- [x] TestVision: raccolta report misura per frame e confronto CAM01/CAM02.
- [x] TestVision: protezione contro broadcast multicamera in `sendOnly`, che poteva bloccare la pipe.
- [x] USB camera: risoluzione manuale opzionale oppure massima automatica.
- [x] USB/Vimba/File setup: `frameIntervalMs` salvato in configurazione camera.
- [x] Build Visual Studio 2022 configurata con preset CMake `x64-release`.
- [x] README collaboratore e manuale helper aggiunti.

## Multicamera e runtime

- [x] Supportare fino a 16 slot camera configurabili.
- [x] Sorgenti miste: file, simulatore, USB, Vimba.
- [x] Runtime asincrono base per camera con isolamento risultati.
- [x] Camera strip e overview per piu' camere attive.
- [ ] Scalare test progressivi: 4-5, 6-7, 8-10, 12-16 camere.
- [ ] Verificare isolamento totale tra camere: ricette, pose, geometrie, misure, overlay.
- [ ] Gestire errori simulatore: connessione assente, timeout, frame duplicati/fuori ordine, restart indipendente.
- [ ] Misurare tempi ciclo per camera e tempi massimi in UI.
- [ ] Ridurre clone immagini, conversioni `matToPixmap` e aggiornamenti inutili in START.

## TestVision e regressioni

- [x] Protocollo named pipe locale tra TestVision e VisionSystemCPP.
- [x] Frame simulati con cameraId, slot, ricetta, strategia e immagine base64.
- [x] Risultati con posa, misure, tempi, validita' e diagnostica.
- [x] Report JSON timestampati.
- [x] Campagne JSON con profilo esecuzione.
- [x] Strategie selezionabili, inclusa localizzazione AI YOLO.
- [ ] Confronto automatico tra due report storici.
- [ ] Report HTML leggibile con OK/NOK, errori numerici, tempi, immagini diagnostiche.
- [ ] Endurance: log completi, crash, timeout, memoria, ripresa dopo arresto.
- [ ] Target test C++ piu' ampio: geometrie, misure, tolleranze, ricette temporanee.
- [ ] Runner non interattivo futuro: `--test scenario.json --output result.json`.

## Geometrie e misure

- [x] Salvataggio geometrie in ricetta camera.
- [x] Overlay diagnostici geometrie rilevate.
- [x] Misure base in pixel e in unita' reali quando disponibile.
- [x] Tolleranze min/max su misure e tabella tolleranze.
- [x] Geometrie costruite: centro, intersezione, proiezione, offset, tangente, linea media.
- [ ] Rendere tutte le geometrie eliminabili da UI.
- [ ] Quando si crea/seleziona una misura, permettere selezione diretta sul disegno oltre alle combo.
- [ ] Test negativi: feature assente deve dare risultato non valido coerente, non intermittente.
- [ ] Test stabilita': stesso frame ripetuto non deve far comparire/scomparire geometrie.
- [ ] Risultati misura: indicare sempre origine scala reale, calibrazione camera o campione.

## Filetto

- [x] ROI filetto e overlay profilo.
- [x] Diametro maggiore/esterno.
- [x] Diametro minore/interno.
- [x] Passo.
- [x] Fase.
- [x] Diametro medio calcolato e disponibile nel runtime.
- [ ] Aggiungere diametro medio alla lista tolleranze se non configurato.
- [ ] Correggere/validare fase: su campione in fase deve tendere a 0 gradi.
- [ ] Migliorare robustezza punti filetto: evitare agganci fuori filetto, soprattutto estremi/imbocchi.
- [ ] Campione SVG in fase e campione SVG fuori fase controllato 5-6 gradi.
- [ ] Diagnostica dedicata: numero creste/gole, punti scartati, motivi invalidita'.

## Calibrazione e metrologia reale

- [x] Calibrazione checkerboard base con pixel/mm.
- [x] Salvataggio calibrazione per camera.
- [x] Conversione misure in mm/gradi quando disponibile.
- [ ] Validare calibrazione checkerboard con camera reale e pezzo reale.
- [ ] Calibrazione completa: matrice camera, distorsione e correzione immagine/punti.
- [ ] Chiarire in UI quando una quota usa calibrazione camera e quando usa campione master.

## Localizzazione e AI

- [x] Localizzazione AI YOLO segmentation base: raw, masks, labels, training, inferenza.
- [x] Centroide, area, contorno, bounding box e orientamento da maschera.
- [x] Riferimenti interni asimmetrici per orientamento su forme simmetriche.
- [ ] Labeling con pennello, gomma, undo/redo e modifica riferimenti.
- [ ] Fallback configurabile alla localizzazione tradizionale.
- [ ] Inferenza ONNX in produzione e confronto prestazioni rispetto a `.pt`.
- [ ] Classificazione: modello attivo unico, confronto vecchio/nuovo, promozione esplicita.
- [ ] Segmentazione/anomaly: completare flusso UI coerente con classificazione.

## Accessi e HMI

- [x] Login e ruoli base Viewer/Operatore/Supervisore/Admin/Guru.
- [x] Configuratore tool opzionali.
- [x] Guru nascosto con opzione riservata.
- [ ] Definire matrice permessi definitiva.
- [ ] Applicare permessi a start/stop, setup, geometrie, misure, calibrazione, ricette, sistema.
- [ ] Logout o cambio livello con PIN/password.
- [ ] Mostrare livello utente corrente in toolbar.
- [ ] Loggare accessi e tentativi negati.

## Ricette e dati

- [x] Ricette per camera con JSON separati.
- [x] Import/export cartella ricetta.
- [x] Duplicazione ricetta.
- [ ] Export/import zip.
- [ ] Separare meglio campione master e immagini test.
- [ ] Pulire ricette demo versionate: tenere solo casi utili e leggeri.
- [ ] Spostare dataset grandi e prove reali fuori dal repo o in storage dedicato.

## Documentazione

- [x] `README.md` per collaboratori.
- [x] `docs/helper_manual.md` per helper/tool interni.
- [x] `docs/programmer_manual.md` esistente.
- [ ] Aggiornare `docs/programmer_manual.md` con USB resolution/timer, TestVision e filettature.
- [ ] Aggiungere guida rapida "come fare una nuova ricetta".
- [ ] Aggiornare help operatore quando un tool cambia comportamento.

## Regole operative

- [ ] Prima di push stabile: build `VisionSystemCPP`, build `VisionSystemTestVision`, `ctest`.
- [ ] Non committare report, log, dump, modelli AI, dataset grandi o prove macchina non ripulite.
- [ ] Ogni nuovo helper riusabile va documentato in `docs/helper_manual.md`.
- [ ] Ogni nuovo tool deve avere una ricetta/test demo piccolo e ripetibile.
- [ ] Commentare solo blocchi non ovvi: formule, convenzioni, fallback, motivi industriali.

---

## English Summary

This is the single project backlog. Keep this file updated instead of creating
separate temporary notes.

### Immediate Priorities

- [ ] Validate the software on the real machine with real camera, lighting,
  vibration and production-like parts.
- [ ] Finish the cleanup before adding collaborators: decide which demo recipes
  are versioned and which real datasets stay local.
- [ ] Stabilize the thread inspection tool, especially phase: an in-phase sample
  should read close to 0 degrees.
- [ ] Fix `Diametro punta` in `stress_test`; it is currently invalid in recent
  multicamera reports.
- [ ] Add/configure tolerance for thread pitch diameter.
- [ ] Rename provisional `measurement_7...measurement_10` in `stress_test`.
- [ ] Validate START/STOP with 2, 4 and 8 cameras.
- [ ] Investigate slow MSBuild behavior when it recompiles many sources in one
  large compiler invocation.

### Recently Done

- [x] Added `stress_test` recipe with geometries, measurements, thread tool and
  sample images.
- [x] Duplicated `stress_test` for multiple cameras.
- [x] Added `midline_between_lines` constructed geometry.
- [x] Highlighted selected source geometries while creating measurements and
  constructed geometries.
- [x] Added thread measurements: major, minor, pitch, phase, pitch diameter.
- [x] Added TestVision CAM01/CAM02 comparison reports.
- [x] Prevented multicamera broadcast in `sendOnly` mode to avoid pipe/UI hangs.
- [x] Added manual USB resolution and saved frame interval settings.
- [x] Added collaborator README and helper manual.

### Rules

- [ ] Before a stable push: build `VisionSystemCPP`, build
  `VisionSystemTestVision`, run `ctest`.
- [ ] Do not commit reports, logs, dumps, AI models, large datasets, or raw
  real-machine captures.
- [ ] Document each reusable helper/tool in `docs/helper_manual.md`.
- [ ] Each new tool should have a small repeatable demo recipe or test.
- [ ] Code comments should explain non-obvious formulas, conventions, fallback
  behavior, or industrial reasons.
