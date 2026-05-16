# TODO - VisionSystemCPP

## Localizzazione BW dimensionale

- [x] Mostrare il tool `Localizzazione` solo per camere con `imageMode=bw` e `inspectionTypes` contenente `dimensional`.
- [x] Attivare il disegno AOE/ROI solo dal pannello `Localizzazione`.
- [x] Disegnare il rettangolo con mouse drag sull'immagine grande.
- [x] Convertire coordinate widget in coordinate immagine originale.
- [x] Salvare la ROI nella ricetta della camera corrispondente.
- [ ] Implementare test localizzazione: sfondo chiaro, pezzo nero, soglia da campionamento angoli.
- [ ] Disegnare contorno pezzo, centro di massa e assi X/Y.
- [ ] Aggiungere maschere/esclusioni per oggetti neri non appartenenti al pezzo.

## Ricette

- [x] Creare struttura iniziale `recipes/default/`.
- [x] Salvare tool camera in `recipes/default/cameras/CAMxx.json`.
- [x] Aggiungere selezione ricetta dal menu superiore.
- [x] Aggiungere creazione/duplicazione/import/export ricetta come cartella.
- [x] Salvare ricetta attiva in `config/app_settings.json`.
- [ ] Aggiungere export/import in formato archivio zip.
