# Promemoria — test multicamera e metrologia

Fase attuale: **debug e ottimizzazione della base** (non integrare ancora tool verticali: filettature, dentate, DIN 471-472).

---

## Obiettivo della giornata

Scalare progressivamente il carico camere e verificare stabilità pose, geometrie e misure.

| Step | Camere | Cosa verificare |
|------|--------|-----------------|
| 1 | 4–5 | Pose, overlay, STOP→START senza reset manuale |
| 2 | 6–7 | UI reattiva, tile sincronizzati, tempi ciclo |
| 3 | 8–10 | Identificare collo di bottiglia (CPU, clone, geometria) |
| 4 | 12–16 | Throughput sostenuto, nessuna camera in ritardo |

**Setup test:** log dettagliato **spento**; TestVision send-only se serve isolare il carico.

---

## Checklist operativa

### Runtime
- [ ] STOP → ultima immagine OK → START → simulatore: posizioni coerenti senza tornare al sample
- [ ] Cerchi/geometrie in spazio pezzo seguono il pezzo in rotazione (non solo vicino alla posizione iniziale)
- [ ] Annotare ms/ciclo e ms/camera a ogni step (4, 6, 8, 12, 16)

### Metrologia (pose vs misure)
- [ ] Su 3–4 angoli (0°, ~15°, ~30°, traslazione leggera): annotare score pose e valori misura
- [ ] Se la misura varia con l’angolo: distinguere errore pose vs taratura edge
- [ ] Affinare edge per camera critica: `bandHalfWidth`, `edgeSensitivity`, `useSubpixel`, filtri derivata/statistici

### CAM03 / rettangolo_con_fori
- [ ] Verificare circle_1–3 e misure dopo fix `anchorInImageSpace`
- [ ] Ridisegnare o risalvare cerchi se ancora in spazio immagine ancorato

---

## Se qualcosa degrada

| Sintomo | Direzione probabile |
|---------|---------------------|
| Tutte le camere rallentano insieme | UI, pixmap, log, lock globali |
| Solo alcune camere in ritardo | Pool async per camera o ricetta pesante |
| Grafica OK, misure lente | Pipeline geometria/misure |

---

## Prossimi step (non domani)

- TestVision: tabella **misura × angolo (°)** per correlazione pose ↔ errore misura (JSON misure già in uscita dal simulatore)
- Ottimizzazioni residue: meno `clone()` per frame, skip `matToPixmap` in griglia START
- Tool verticali solo dopo base stabile a 16 camere

---

## Build

```text
cmake --build build --config Release --target VisionSystemCPP
```

Eseguibile: `build/Release/VisionSystemCPP.exe`
