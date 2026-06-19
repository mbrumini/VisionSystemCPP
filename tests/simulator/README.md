# Protocollo simulatore VisionSystem

VisionSystemCPP espone il named pipe Windows:

```text
\\.\pipe\VisionSystemSimulator
```

Il protocollo corrente e' JSON Lines UTF-8: un oggetto JSON per riga.
Le immagini sono PNG codificate Base64.

## Frame

```json
{
  "type": "frame",
  "protocolVersion": 1,
  "scenarioId": "smoke-test",
  "cameraId": "CAM01",
  "channel": "CAM01",
  "slot": 1,
  "frameId": 1,
  "timestamp": "2026-06-19T08:00:00.000+02:00",
  "imageFormat": "png",
  "imageBase64": "..."
}
```

Vision risponde subito con `frameAccepted`. Quando lo slot configurato con lo
stesso canale acquisisce ed elabora il frame, invia un secondo messaggio
`result` con lo stesso `scenarioId`, `cameraId`, `channel` e `frameId`.

## Test rapido

Avviare VisionSystemCPP e poi:

```powershell
powershell -ExecutionPolicy Bypass -File tests/simulator/smoke_pipe.ps1
```

Il test verifica handshake, ping/pong e accettazione di un'immagine PNG.

## Primo scenario TestVision

Il target GUI `VisionSystemTestVision` genera la croce asimmetrica master,
applica le rotazioni definite nello scenario, invia i frame e produce un
report JSON.

Prima dell'esecuzione:

1. In VisionSystemCPP aprire `Sistema -> Configurazione telecamere`.
2. Impostare `CAM01` come `simulator`, canale `CAM01`.
3. Selezionare CAM01 e avviare la camera.

Eseguire:

```powershell
build\Release\VisionSystemTestVision.exe tests\scenarios\cross_rotation_cam01.json
```

La finestra non avvia automaticamente la sequenza. Premere `Start` in
TestVision. TestVision invia un solo frame e attende il risultato con lo stesso
`frameId` prima di passare al successivo. `Stop` interrompe la sequenza.

In Vision, la camera simulata deve avere Grab/Start attivo. Se non e' attivo,
TestVision resta fermo sul frame corrente con lo stato
`attendo il risultato di Vision`.

File principali:

- scenario: `tests/scenarios/cross_rotation_cam01.json`;
- master versionato: `tests/assets/cross_asymmetric/master.png`;
- report runtime, escluso da Git: `tests/reports/cross_rotation_cam01.json`.

Se Vision accetta i frame ma lo slot non e' configurato o avviato come
simulatore, ogni frame termina con `result_timeout` e una spiegazione nel
report.
