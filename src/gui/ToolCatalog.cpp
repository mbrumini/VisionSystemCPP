#include "ToolCatalog.h"

#include <QHash>

namespace
{
using ActionList = QVector<ToolActionDefinition>;

ToolDefinition makeTool(const QString& id, const QString& label, const ActionList& actions)
{
  return {id, label, actions};
}

const QHash<QString, ToolDefinition>& tools()
{
  static const QHash<QString, ToolDefinition> catalog = {
    {"measurements", makeTool("measurements", "Misure", {
      {"diameter", "Diametro"},
      {"length", "Lunghezza"},
      {"radius", "Raggio"},
      {"distance", "Distanza"},
      {"angle", "Angolo"},
      {"center", "Centro"},
      {"area", "Area"}
    })},
    {"dimensions", makeTool("dimensions", "Dimensioni", {
      {"outerDiameter", "Diametro esterno"},
      {"innerDiameter", "Diametro interno"},
      {"width", "Larghezza"},
      {"height", "Altezza"},
      {"length", "Lunghezza"},
      {"radius", "Raggio"},
      {"parallelism", "Parallelismo"}
    })},
    {"threshold", makeTool("threshold", "Soglia", {
      {"manualThreshold", "Soglia manuale"},
      {"autoThreshold", "Soglia automatica"},
      {"minValue", "Valore minimo"},
      {"maxValue", "Valore massimo"},
      {"invert", "Inverti BW"}
    })},
    {"calibration", makeTool("calibration", "Calibrazione", {
      {"pixelSize", "Scala pixel/mm"},
      {"referenceMeasure", "Quota riferimento"},
      {"calibrationImage", "Immagine calibrazione"},
      {"saveCalibration", "Salva calibrazione"}
    })},
    {"roi", makeTool("roi", "ROI", {
      {"rectangle", "Rettangolo"},
      {"circle", "Cerchio"},
      {"polygon", "Poligono"},
      {"move", "Sposta"},
      {"resize", "Ridimensiona"},
      {"clear", "Cancella ROI"}
    })},
    {"saveSample", makeTool("saveSample", "Salva campione", {
      {"saveOk", "Salva OK"},
      {"saveNok", "Salva NOK"},
      {"saveRaw", "Salva originale"},
      {"saveProcessed", "Salva processata"}
    })},
    {"tolerances", makeTool("tolerances", "Tolleranze", {
      {"nominal", "Nominale"},
      {"minTolerance", "Tolleranza min"},
      {"maxTolerance", "Tolleranza max"},
      {"okNokRule", "Regola OK/NOK"}
    })},
    {"surfaceDefects", makeTool("surfaceDefects", "Difetti superficie", {
      {"scratch", "Graffi"},
      {"spot", "Macchie"},
      {"dent", "Ammaccature"},
      {"blob", "Blob"},
      {"minimumArea", "Area minima"}
    })},
    {"lighting", makeTool("lighting", "Illuminazione", {
      {"brightness", "Luminosita'"},
      {"uniformity", "Uniformita'"},
      {"exposure", "Esposizione"},
      {"gain", "Gain"}
    })},
    {"contrast", makeTool("contrast", "Contrasto", {
      {"contrastLevel", "Livello contrasto"},
      {"gamma", "Gamma"},
      {"equalize", "Equalizza"},
      {"clahe", "CLAHE"}
    })},
    {"defectMap", makeTool("defectMap", "Mappa difetti", {
      {"heatMap", "Heat map"},
      {"defectList", "Lista difetti"},
      {"severity", "Severita'"},
      {"exportMap", "Esporta mappa"}
    })},
    {"aiModel", makeTool("aiModel", "Modello AI", {
      {"selectModel", "Seleziona modello"},
      {"loadModel", "Carica modello"},
      {"classMap", "Mappa classi"},
      {"runInference", "Test inferenza"}
    })},
    {"confidence", makeTool("confidence", "Confidenza", {
      {"minimumConfidence", "Confidenza minima"},
      {"warningThreshold", "Soglia warning"},
      {"rejectThreshold", "Soglia scarto"}
    })},
    {"classes", makeTool("classes", "Classi", {
      {"okClass", "Classe OK"},
      {"nokClass", "Classe NOK"},
      {"ignoreClass", "Classi ignorate"},
      {"classColors", "Colori classi"}
    })},
    {"datasetCapture", makeTool("datasetCapture", "Cattura dataset", {
      {"captureOk", "Cattura OK"},
      {"captureNok", "Cattura NOK"},
      {"captureUnknown", "Cattura incerti"},
      {"openDatasetFolder", "Apri cartella dataset"}
    })}
  };

  return catalog;
}
}

ToolDefinition ToolCatalog::tool(const QString& toolId)
{
  const ToolDefinition fallback = makeTool(toolId, toolId, {
    {"placeholder", "Placeholder"}
  });

  return tools().value(toolId, fallback);
}

QString ToolCatalog::label(const QString& toolId)
{
  return tool(toolId).label;
}
