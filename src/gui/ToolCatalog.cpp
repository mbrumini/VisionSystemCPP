#include "ToolCatalog.h"

#include <QHash>

namespace
{
using ActionList = QVector<ToolActionDefinition>;

struct ToolActionTemplate
{
  QString id;
  QString textKey;
};

struct ToolTemplate
{
  QString id;
  QString textKey;
  QVector<ToolActionTemplate> actions;
};

ToolTemplate makeTool(const QString& id, const QString& textKey, const QVector<ToolActionTemplate>& actions)
{
  return {id, textKey, actions};
}

const QHash<QString, ToolTemplate>& tools()
{
  static const QHash<QString, ToolTemplate> catalog = {
    {"localization", makeTool("localization", "tools.localization", {
      {"searchRoi", "actions.searchRoi"},
      {"addExclusion", "actions.addExclusion"},
      {"clearExclusions", "actions.clearExclusions"},
      {"testLocalization", "actions.testLocalization"},
      {"clearRoi", "actions.clearRoi"}
    })},
    {"geometries", makeTool("geometries", "tools.geometries", {
      {"pointGeometry", "actions.pointGeometry"},
      {"lineGeometry", "actions.lineGeometry"},
      {"circleGeometry", "actions.circleGeometry"},
      {"arcGeometry", "actions.arcGeometry"},
      {"edgeGeometry", "actions.edgeGeometry"},
      {"contourGeometry", "actions.contourGeometry"}
    })},
    {"measurements", makeTool("measurements", "tools.measurements", {
      {"diameter", "actions.diameter"},
      {"length", "actions.length"},
      {"radius", "actions.radius"},
      {"distance", "actions.distance"},
      {"angle", "actions.angle"},
      {"center", "actions.center"},
      {"area", "actions.area"}
    })},
    {"dimensions", makeTool("dimensions", "tools.dimensions", {
      {"outerDiameter", "actions.outerDiameter"},
      {"innerDiameter", "actions.innerDiameter"},
      {"width", "actions.width"},
      {"height", "actions.height"},
      {"length", "actions.length"},
      {"radius", "actions.radius"},
      {"parallelism", "actions.parallelism"}
    })},
    {"threshold", makeTool("threshold", "tools.threshold", {
      {"manualThreshold", "actions.manualThreshold"},
      {"autoThreshold", "actions.autoThreshold"},
      {"minValue", "actions.minValue"},
      {"maxValue", "actions.maxValue"},
      {"invert", "actions.invert"}
    })},
    {"calibration", makeTool("calibration", "tools.calibration", {
      {"pixelSize", "actions.pixelSize"},
      {"referenceMeasure", "actions.referenceMeasure"},
      {"calibrationImage", "actions.calibrationImage"},
      {"saveCalibration", "actions.saveCalibration"}
    })},
    {"roi", makeTool("roi", "tools.roi", {
      {"rectangle", "actions.rectangle"},
      {"circle", "actions.circle"},
      {"polygon", "actions.polygon"},
      {"move", "actions.move"},
      {"resize", "actions.resize"},
      {"clear", "actions.clearRoi"}
    })},
    {"saveSample", makeTool("saveSample", "tools.saveSample", {
      {"saveOk", "actions.saveOk"},
      {"saveNok", "actions.saveNok"},
      {"saveRaw", "actions.saveRaw"},
      {"saveProcessed", "actions.saveProcessed"}
    })},
    {"tolerances", makeTool("tolerances", "tools.tolerances", {
      {"nominal", "actions.nominal"},
      {"minTolerance", "actions.minTolerance"},
      {"maxTolerance", "actions.maxTolerance"},
      {"okNokRule", "actions.okNokRule"}
    })},
    {"surfaceLocalization", makeTool("surfaceLocalization", "tools.surfaceLocalization", {
      {"surfaceOuterCircle", "actions.surfaceOuterCircle"},
      {"surfaceInnerCircle", "actions.surfaceInnerCircle"},
      {"surfaceAddExclusion", "actions.surfaceAddExclusion"},
      {"surfaceClearExclusions", "actions.surfaceClearExclusions"}
    })},
    {"surfaceDefects", makeTool("surfaceDefects", "tools.surfaceDefects", {
      {"scratch", "actions.scratch"},
      {"spot", "actions.spot"},
      {"dent", "actions.dent"},
      {"blob", "actions.blob"},
      {"minimumArea", "actions.minimumArea"}
    })},
    {"lighting", makeTool("lighting", "tools.lighting", {
      {"brightness", "actions.brightness"},
      {"uniformity", "actions.uniformity"},
      {"exposure", "actions.exposure"},
      {"gain", "actions.gain"}
    })},
    {"contrast", makeTool("contrast", "tools.contrast", {
      {"contrastLevel", "actions.contrastLevel"},
      {"gamma", "actions.gamma"},
      {"equalize", "actions.equalize"},
      {"clahe", "actions.clahe"}
    })},
    {"defectMap", makeTool("defectMap", "tools.defectMap", {
      {"heatMap", "actions.heatMap"},
      {"defectList", "actions.defectList"},
      {"severity", "actions.severity"},
      {"exportMap", "actions.exportMap"}
    })},
    {"aiModel", makeTool("aiModel", "tools.aiModel", {
      {"selectModel", "actions.selectModel"},
      {"loadModel", "actions.loadModel"},
      {"classMap", "actions.classMap"},
      {"runInference", "actions.runInference"}
    })},
    {"ai", makeTool("ai", "tools.ai", {
      {"aiClassification", "tools.aiClassification"},
      {"aiAnomaly", "tools.aiAnomaly"},
      {"aiSegmentation", "tools.aiSegmentation"}
    })},
    {"aiClassification", makeTool("aiClassification", "tools.aiClassification", {
      {"acquireAiRawImage", "actions.acquireAiRawImage"},
      {"addAiClass", "actions.addAiClass"},
      {"prepareAiDataset", "actions.prepareAiDataset"},
      {"trainAiModel", "actions.trainAiModel"}
    })},
    {"confidence", makeTool("confidence", "tools.confidence", {
      {"minimumConfidence", "actions.minimumConfidence"},
      {"warningThreshold", "actions.warningThreshold"},
      {"rejectThreshold", "actions.rejectThreshold"}
    })},
    {"classes", makeTool("classes", "tools.classes", {
      {"okClass", "actions.okClass"},
      {"nokClass", "actions.nokClass"},
      {"ignoreClass", "actions.ignoreClass"},
      {"classColors", "actions.classColors"}
    })},
    {"datasetCapture", makeTool("datasetCapture", "tools.datasetCapture", {
      {"captureOk", "actions.captureOk"},
      {"captureNok", "actions.captureNok"},
      {"captureUnknown", "actions.captureUnknown"},
      {"openDatasetFolder", "actions.openDatasetFolder"}
    })}
  };

  return catalog;
}
}

ToolDefinition ToolCatalog::tool(const QString& toolId, const TranslationManager& translations)
{
  const ToolTemplate toolTemplate = tools().value(toolId, makeTool(toolId, toolId, {
    {"placeholder", "actions.placeholder"}
  }));

  ToolDefinition result;
  result.id = toolTemplate.id;
  result.label = translations.text(toolTemplate.textKey);

  for (const ToolActionTemplate& actionTemplate : toolTemplate.actions)
  {
    result.actions.append({actionTemplate.id, translations.text(actionTemplate.textKey)});
  }

  return result;
}

QString ToolCatalog::label(const QString& toolId, const TranslationManager& translations)
{
  return tool(toolId, translations).label;
}
