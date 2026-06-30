# Appunti Auto Geometry Assistant

Obiettivo: assistente esterno, separato da VisionSystemCPP, che aiuta a creare una bozza ricetta partendo da immagine campione e, in futuro, da PDF tecnico.

## Principio base

L'assistente non deve mai scrivere direttamente nella ricetta definitiva.

Deve produrre una bozza:

- localizzazione proposta
- geometrie candidate
- misure candidate
- min/max e tolleranze, se ricavate da PDF
- immagine preview con overlay

VisionSystemCPP importa solo quello che l'operatore conferma.

## Flusso immagine

1. Vision acquisisce l'immagine campione.
2. Vision esporta immagine e contesto in una richiesta.
3. L'assistente legge l'immagine.
4. OpenCV/algoritmi classici trovano primitive robuste:
   - contorno pezzo
   - centro/rotazione/scala
   - cerchi
   - linee
   - archi
   - fori
   - coppie di bordi misurabili
5. Ollama puo' aiutare a scegliere cosa ha senso proporre.
6. L'assistente genera:
   - `proposal.json`
   - `preview_overlay.png`
7. L'operatore conferma solo le misure utili.
8. Vision importa le confermate e l'operatore rifinisce edge, ROI, soglie e posizione.

## Flusso PDF

Se l'operatore carica un PDF tecnico, l'assistente puo' usarlo per estrarre:

- quote nominali
- min/max
- tolleranze
- unita'
- diametri
- raggi
- filetti
- note di controllo
- riferimenti/datum, quando interpretabili

Esempio:

```json
{
  "label": "Diametro esterno",
  "nominal": 20.0,
  "min": 19.95,
  "max": 20.05,
  "unit": "mm",
  "source": {
    "document": "disegno_cliente.pdf",
    "page": 1,
    "text": "Ø20 ±0.05"
  }
}
```

## Matching PDF-immagine

L'assistente deve provare ad associare le quote lette dal PDF alle geometrie viste sull'immagine.

Esempio:

- `Ø20 ±0.05` -> cerchio esterno
- `Ø8 H7` -> foro interno
- `35 ±0.1` -> distanza tra due linee estreme

Questa associazione deve sempre essere confermata dall'operatore.

## Ruolo di Ollama

Ollama non deve fare la misura precisa in pixel.

Uso corretto:

- leggere e sintetizzare il contesto
- interpretare quote e note del PDF
- scegliere tra candidate gia' calcolate
- ridurre proposte inutili
- spiegare perche' una misura e' stata suggerita
- imparare gradualmente dalle conferme/scarti dell'operatore

Uso da evitare:

- decidere quote definitive senza conferma
- scrivere ricette
- inventare tolleranze mancanti
- usare geometrie non supportate da evidenza immagine/PDF

## Memoria futura

Ogni conferma o scarto puo' essere salvato come feedback:

- immagine campione
- geometrie proposte
- misure proposte
- misure confermate
- misure scartate
- correzioni manuali
- PDF sorgente
- ruolo della camera/profilo

Col tempo l'assistente puo' imparare preferenze operative:

- massimo numero di misure da proporre
- quote spesso accettate per famiglie simili
- candidate spesso scartate
- abbinamenti PDF-immagine piu' probabili

## Regola di sicurezza

Ogni dato importato da assistente deve essere visibile e modificabile prima del salvataggio in ricetta.

Niente automatismi ciechi.
