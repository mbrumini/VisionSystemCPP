# Auto Geometry Assistant Demo

Demo esterna e separata da VisionSystemCPP.

Questa cartella serve solo per provare l'idea dell'assistente che propone geometrie da un'immagine campione. Non e' collegata al programma, non modifica ricette reali e non viene compilata da CMake.

## Avvio

```powershell
python experiments\auto_geometry_assistant\server.py
```

Poi apri:

```text
http://127.0.0.1:8765/
```

## API di prova

```text
POST http://127.0.0.1:8765/propose-geometries
Content-Type: application/json
```

Esempio:

```json
{
  "cameraId": "CAM01",
  "recipeId": "demo",
  "sampleImage": "sample.png"
}
```

Risposta demo:

```json
{
  "status": "ok",
  "geometries": [],
  "measurements": [],
  "note": "Demo stub: nessuna analisi immagine ancora implementata."
}
```

## Regola importante

L'assistente deve proporre una bozza. VisionSystemCPP, in futuro, dovra' far confermare all'utente prima di salvare qualunque geometria in ricetta.

Vedi anche [APPUNTI.md](APPUNTI.md) per il flusso futuro con immagine campione, Ollama e PDF tecnico.
