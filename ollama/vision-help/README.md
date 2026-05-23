# VisionSystem Help con Ollama

Questa cartella prepara un assistente locale per il manuale di VisionSystemCPP.

Non e un fine-tuning vero del modello: Ollama usa un modello base con istruzioni dedicate, mentre lo script `tools/vision_help_query.py` recupera i file piu pertinenti da `docs/help/it` e li passa al modello come contesto.

## Setup rapido

```powershell
ollama pull llama3.2:3b
ollama create vision-help -f ollama/vision-help/Modelfile
```

## Domande al manuale

```powershell
python tools/vision_help_query.py "come imposto una luce per una misura BN?"
python tools/vision_help_query.py "perche in setup vedo edge ma non misure?"
python tools/vision_help_query.py "quando uso le finestre rosse di esclusione?"
```

Lo script stampa prima i file consultati e poi la risposta generata da Ollama.

## Evoluzione verso training vero

Quando i testi sono maturi, si puo generare un dataset istruzione/risposta in JSONL dai file `docs/help/it/functions`.
Quel dataset puo essere usato per fine-tuning esterno, poi il modello risultante si puo convertire in GGUF e caricare in Ollama.

```powershell
python tools/build_vision_help_dataset.py
```

Output:

```text
ollama/vision-help/vision_help_dataset.jsonl
```
