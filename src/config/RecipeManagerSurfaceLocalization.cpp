#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPolygon>

#include "RecipeJsonUtils.h"

using namespace RecipeJsonUtils;

bool RecipeManager::clearSurfaceLocalizationGeometry(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  const QString method = surfaceLocalization.value("method").toString("threshold");
  surfaceLocalization = QJsonObject();
  surfaceLocalization["enabled"] = true;
  surfaceLocalization["method"] = method;
  surfaceLocalization["exclusionRects"] = QJsonArray();

  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}
SurfaceLocalizationStrategyConfig RecipeManager::loadSurfaceLocalizationStrategy(const QString& cameraId) const
{
  SurfaceLocalizationStrategyConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject strategy = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("strategy").toObject();

  if (strategy.isEmpty())
  {
    return config;
  }

  config.name = strategy.value("name").toString(config.name);
  config.origin = strategy.value("origin").toString(config.origin);

  const QJsonObject xAxis = strategy.value("xAxis").toObject();
  config.xAxisFrom = xAxis.value("from").toString();
  config.xAxisTo = xAxis.value("to").toString();

  const QJsonArray features = strategy.value("features").toArray();

  for (const QJsonValue& value : features)
  {
    const QJsonObject featureObject = value.toObject();
    SurfaceStrategyFeatureConfig feature;
    feature.id = featureObject.value("id").toString();
    feature.polarity = featureObject.value("polarity").toString(feature.polarity);
    feature.searchRoi = rectFromJson(featureObject.value("searchRoi").toObject()).normalized();

    const QJsonObject threshold = featureObject.value("threshold").toObject();
    feature.thresholdMin = threshold.value("min").toInt(feature.thresholdMin);
    feature.thresholdMax = threshold.value("max").toInt(feature.thresholdMax);

    const QJsonObject expectedRadius = featureObject.value("expectedRadius").toObject();
    feature.expectedRadiusMin = expectedRadius.value("min").toDouble(feature.expectedRadiusMin);
    feature.expectedRadiusMax = expectedRadius.value("max").toDouble(feature.expectedRadiusMax);

    if (!feature.id.isEmpty() && feature.searchRoi.isValid())
    {
      config.features.append(feature);
    }
  }

  return config;
}

SurfaceAnnulusLocalizationConfig RecipeManager::loadSurfaceAnnulusLocalization(const QString& cameraId) const
{
  SurfaceAnnulusLocalizationConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject surfaceLocalization = root.value("tools").toObject()
    .value("surfaceLocalization").toObject();
  config.method = surfaceLocalization.value("method").toString(config.method);

  if (config.method == "grayscale_threshold" || config.method == "grayscale_annulus_threshold")
  {
    config.method = "threshold";
  }

  if (config.method == "grayscale_annulus_edge")
  {
    config.method = "edge";
  }

  const QJsonObject annulus = surfaceLocalization.value("annulus").toObject();

  const QJsonObject outer = annulus.value("outerCircle").toObject();
  const QJsonObject inner = annulus.value("innerCircle").toObject();

  if (!outer.isEmpty())
  {
    config.hasOuterCircle = true;
    config.center = QPoint(outer.value("centerX").toInt(), outer.value("centerY").toInt());
    config.outerRadius = qMax(1, outer.value("radius").toInt());
  }

  if (!inner.isEmpty())
  {
    config.innerRadius = inner.value("radius").toInt();

    if (!config.hasOuterCircle)
    {
      config.center = QPoint(inner.value("centerX").toInt(), inner.value("centerY").toInt());
    }

    if (config.innerRadius > 0)
    {
      config.hasInnerCircle = true;
      if (config.hasOuterCircle && config.innerRadius >= config.outerRadius)
      {
        config.innerRadius = qMax(1, config.outerRadius - 1);
        config.hasInnerCircle = config.outerRadius > 1;
      }
    }
  }

  const QJsonObject threshold = surfaceLocalization.value("threshold").toObject();
  config.thresholdMin = threshold.value("min").toInt(config.thresholdMin);
  config.thresholdMax = threshold.value("value").toInt(threshold.value("max").toInt(config.thresholdMax));

  const QJsonObject edge = surfaceLocalization.value("edge").toObject();
  config.edgeSensitivity = edge.value("sensitivity").toInt(config.edgeSensitivity);
  config.edgeFitMaxError = edge.value("fitMaxError").toInt(config.edgeFitMaxError);
  config.pcaResolveAmbiguity = edge.value("pcaResolveAmbiguity").toBool(config.pcaResolveAmbiguity);

  const QJsonObject edgeCircle = edge.value("circle").toObject();

  if (!edgeCircle.isEmpty())
  {
    config.hasEdgeCircle = true;
    config.edgeCenter = QPoint(edgeCircle.value("centerX").toInt(), edgeCircle.value("centerY").toInt());
    config.edgeRadius = qMax(1, edgeCircle.value("radius").toInt());
  }

  const QJsonObject band = edge.value("band").toObject();
  config.edgeBandInner = band.value("inner").toInt(config.edgeBandInner);
  config.edgeBandOuter = band.value("outer").toInt(config.edgeBandOuter);
  config.edgeBandInner = qBound(1, config.edgeBandInner, config.hasEdgeCircle ? qMax(1, config.edgeRadius - 1) : 200);
  config.edgeBandOuter = qBound(1, config.edgeBandOuter, 200);
  return config;
}

bool RecipeManager::saveSurfaceAnnulusCircle(const QString& cameraId, bool outerCircle, const QPoint& center, int radius, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "threshold");

  const SurfaceAnnulusLocalizationConfig current = loadSurfaceAnnulusLocalization(cameraId);
  int safeRadius = qMax(1, radius);
  if (outerCircle && current.hasInnerCircle)
  {
    safeRadius = qMax(safeRadius, current.innerRadius + 1);
  }
  else if (!outerCircle && current.hasOuterCircle)
  {
    safeRadius = qMax(1, qMin(safeRadius, current.outerRadius - 1));
  }

  QJsonObject annulus = surfaceLocalization.value("annulus").toObject();
  annulus[outerCircle ? "outerCircle" : "innerCircle"] = circleToJson(center, safeRadius);
  surfaceLocalization["annulus"] = annulus;

  QJsonObject threshold = surfaceLocalization.value("threshold").toObject();

  if (!threshold.contains("min"))
  {
    threshold["min"] = SurfaceDefectSettings().thresholdMin;
  }

  if (!threshold.contains("max"))
  {
    threshold["max"] = SurfaceDefectSettings().thresholdMax;
  }

  if (!threshold.contains("value"))
  {
    threshold["value"] = threshold.value("max").toInt(SurfaceDefectSettings().thresholdMax);
  }

  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceAnnulusThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "threshold");

  QJsonObject threshold;
  threshold["min"] = qBound(0, minValue, 255);
  threshold["max"] = qBound(threshold.value("min").toInt(), maxValue, 255);
  threshold["value"] = threshold.value("max").toInt();
  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceLocalizationMethod(const QString& cameraId, const QString& method, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, method);
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeCircle(const QString& cameraId, const QPoint& center, int radius, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "edge");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["circle"] = circleToJson(center, qMax(1, radius));

  QJsonObject band = edge.value("band").toObject();

  if (!band.contains("inner"))
  {
    band["inner"] = SurfaceAnnulusLocalizationConfig().edgeBandInner;
  }

  if (!band.contains("outer"))
  {
    band["outer"] = SurfaceAnnulusLocalizationConfig().edgeBandOuter;
  }

  edge["band"] = band;
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  pruneSurfaceLocalizationForMethod(surfaceLocalization, currentMethod == "edgePca" ? currentMethod : "edge");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["sensitivity"] = qBound(0, sensitivity, 255);
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeBand(const QString& cameraId, int innerWidth, int outerWidth, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  pruneSurfaceLocalizationForMethod(surfaceLocalization, currentMethod == "edgePca" ? currentMethod : "edge");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  QJsonObject band;
  const SurfaceAnnulusLocalizationConfig current = loadSurfaceAnnulusLocalization(cameraId);
  const int maxInnerWidth = current.hasEdgeCircle ? qMax(1, current.edgeRadius - 1) : 200;
  band["inner"] = qBound(1, innerWidth, maxInnerWidth);
  band["outer"] = qBound(1, outerWidth, 200);
  edge["band"] = band;
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceEdgeFitMaxError(const QString& cameraId, int maxError, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  pruneSurfaceLocalizationForMethod(surfaceLocalization, currentMethod == "edgePca" ? currentMethod : "edge");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["fitMaxError"] = qBound(1, maxError, 50);
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfacePcaResolveAmbiguity(const QString& cameraId, bool resolve, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString currentMethod = surfaceLocalization.value("method").toString("edge");
  pruneSurfaceLocalizationForMethod(surfaceLocalization, currentMethod == "edgePca" ? currentMethod : "edge");

  QJsonObject edge = surfaceLocalization.value("edge").toObject();
  edge["pcaResolveAmbiguity"] = resolve;
  surfaceLocalization["edge"] = edge;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}


SurfaceModelConfig RecipeManager::loadSurfaceModel(const QString& cameraId) const
{
  SurfaceModelConfig config;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return config;
  }

  const QJsonObject model = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("model").toObject();

  if (model.isEmpty())
  {
    return config;
  }

  config.searchRoi = rectFromJson(model.value("searchRoi").toObject()).normalized();
  const QString storedTemplatePath = model.value("templateImagePath").toString();
  if (!storedTemplatePath.isEmpty())
  {
    const QFileInfo storedTemplateInfo(storedTemplatePath);
    if (storedTemplateInfo.isAbsolute())
    {
      config.templateImagePath = storedTemplatePath;
      if (!storedTemplateInfo.exists())
      {
        const QString portableFallback = surfaceModelTemplateImagePath(cameraId);
        if (QFileInfo::exists(portableFallback))
        {
          config.templateImagePath = portableFallback;
        }
      }
    }
    else
    {
      const QString recipeDirectory = QDir(recipesRoot()).filePath(m_recipeId);
      config.templateImagePath = QDir(recipeDirectory).filePath(storedTemplatePath);
    }
  }
  config.edgeSensitivity = model.value("edgeSensitivity").toInt(config.edgeSensitivity);
  config.maxShapeDistance = model.value("maxShapeDistance").toDouble(config.maxShapeDistance);
  config.minTemplateScore = model.value("minTemplateScore").toDouble(config.minTemplateScore);
  config.useConvexHull = model.value("useConvexHull").toBool(config.useConvexHull);
  config.modelUseEdges = model.value("modelUseEdges").toBool(config.modelUseEdges);

  const QJsonObject angleRange = model.value("angleRange").toObject();
  config.angleStartDegrees = angleRange.value("start").toDouble(config.angleStartDegrees);
  config.angleEndDegrees = angleRange.value("end").toDouble(config.angleEndDegrees);
  config.angleStepDegrees = angleRange.value("step").toDouble(config.angleStepDegrees);

  const QJsonArray contour = model.value("contour").toArray();
  for (const QJsonValue& value : contour)
  {
    config.contour.append(pointFromJson(value.toObject()));
  }

  config.hasModel = config.searchRoi.isValid() && !config.contour.isEmpty() && !config.templateImagePath.isEmpty();
  return config;
}

bool RecipeManager::saveSurfaceModel(const QString& cameraId, const QRect& searchRoi, const QVector<QPoint>& contour, const QString& templateImagePath, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");

  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["searchRoi"] = rectToJson(searchRoi.normalized());
  const QDir recipeDirectory(QDir(recipesRoot()).filePath(m_recipeId));
  QString portableTemplatePath = recipeDirectory.relativeFilePath(QFileInfo(templateImagePath).absoluteFilePath());
  if (portableTemplatePath.startsWith("../"))
  {
    portableTemplatePath = templateImagePath;
  }
  model["templateImagePath"] = QDir::fromNativeSeparators(portableTemplatePath);
  model["edgeSensitivity"] = model.value("edgeSensitivity").toInt(SurfaceModelConfig().edgeSensitivity);
  model["maxShapeDistance"] = model.value("maxShapeDistance").toDouble(SurfaceModelConfig().maxShapeDistance);
  model["minTemplateScore"] = model.value("minTemplateScore").toDouble(SurfaceModelConfig().minTemplateScore);
  model["useConvexHull"] = model.value("useConvexHull").toBool(SurfaceModelConfig().useConvexHull);
  model["modelUseEdges"] = model.value("modelUseEdges").toBool(SurfaceModelConfig().modelUseEdges);

  QJsonObject angleRange = model.value("angleRange").toObject();
  if (!angleRange.contains("start"))
  {
    angleRange["start"] = SurfaceModelConfig().angleStartDegrees;
  }
  if (!angleRange.contains("end"))
  {
    angleRange["end"] = SurfaceModelConfig().angleEndDegrees;
  }
  if (!angleRange.contains("step"))
  {
    angleRange["step"] = SurfaceModelConfig().angleStepDegrees;
  }
  model["angleRange"] = angleRange;

  QJsonArray contourArray;
  for (const QPoint& point : contour)
  {
    contourArray.append(pointToJson(point));
  }
  model["contour"] = contourArray;

  surfaceLocalization["model"] = model;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelEdgeSensitivity(const QString& cameraId, int sensitivity, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["edgeSensitivity"] = qBound(1, sensitivity, 255);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelMaxShapeDistance(const QString& cameraId, double distance, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["maxShapeDistance"] = qBound(0.001, distance, 5.0);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelUseConvexHull(const QString& cameraId, bool useConvexHull, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["useConvexHull"] = useConvexHull;
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelUseEdges(const QString& cameraId, bool useEdges, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["modelUseEdges"] = useEdges;
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelMinTemplateScore(const QString& cameraId, double score, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  model["minTemplateScore"] = qBound(0.0, score, 1.0);
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceModelAngleRange(const QString& cameraId, double startDegrees, double endDegrees, double stepDegrees, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;
  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "model");
  QJsonObject model = surfaceLocalization.value("model").toObject();
  QJsonObject angleRange;
  angleRange["start"] = qBound(-180.0, startDegrees, 180.0);
  angleRange["end"] = qBound(-180.0, endDegrees, 180.0);
  angleRange["step"] = qBound(1.0, stepDegrees, 45.0);
  model["angleRange"] = angleRange;
  surfaceLocalization["model"] = model;
  surfaceLocalization["enabled"] = true;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

QString RecipeManager::surfaceModelTemplateImagePath(const QString& cameraId) const
{
  return QDir(recipesRoot()).filePath(m_recipeId + "/assets/" + cameraId + "_surface_model_template.png");
}

bool RecipeManager::saveSurfaceLocalizationStrategy(const QString& cameraId, const SurfaceLocalizationStrategyConfig& config, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceLocalization, "two_circles_axis");

  QJsonObject strategy;
  strategy["name"] = config.name;
  strategy["origin"] = config.origin;

  QJsonObject xAxis;
  xAxis["from"] = config.xAxisFrom;
  xAxis["to"] = config.xAxisTo;
  strategy["xAxis"] = xAxis;

  QJsonArray features;
  for (const SurfaceStrategyFeatureConfig& feature : config.features)
  {
    QJsonObject featureObject;
    featureObject["id"] = feature.id;
    featureObject["type"] = "circle";
    featureObject["polarity"] = feature.polarity;
    featureObject["searchRoi"] = rectToJson(feature.searchRoi);

    QJsonObject threshold;
    threshold["min"] = feature.thresholdMin;
    threshold["max"] = feature.thresholdMax;
    featureObject["threshold"] = threshold;

    QJsonObject expectedRadius;
    expectedRadius["min"] = feature.expectedRadiusMin;
    expectedRadius["max"] = feature.expectedRadiusMax;
    featureObject["expectedRadius"] = expectedRadius;

    features.append(featureObject);
  }
  strategy["features"] = features;

  surfaceLocalization["strategy"] = strategy;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceStrategyCircle(const QString& cameraId, const QString& featureId, const QPoint& center, int radius, QString* errorMessage) const
{
  SurfaceLocalizationStrategyConfig config = loadSurfaceLocalizationStrategy(cameraId);
  config.name = "two_circles_axis";
  if (config.origin.isEmpty())
  {
    config.origin = "midpoint";
  }
  if (config.xAxisFrom.isEmpty())
  {
    config.xAxisFrom = "circle_a";
  }
  if (config.xAxisTo.isEmpty())
  {
    config.xAxisTo = "circle_b";
  }

  int featureIndex = -1;
  for (int i = 0; i < config.features.size(); ++i)
  {
    if (config.features[i].id == featureId)
    {
      featureIndex = i;
      break;
    }
  }

  SurfaceStrategyFeatureConfig feature;
  if (featureIndex >= 0)
  {
    feature = config.features[featureIndex];
  }
  else
  {
    feature.id = featureId;
    feature.polarity = "NB";
    feature.thresholdMin = 0;
    feature.thresholdMax = 80;
  }

  feature.searchRoi = QRect(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
  feature.expectedRadiusMin = qMax(1.0, radius * 0.7);
  feature.expectedRadiusMax = radius * 1.3;

  if (featureIndex >= 0)
  {
    config.features[featureIndex] = feature;
  }
  else
  {
    if (config.features.isEmpty() && featureId == "circle_b")
    {
      SurfaceStrategyFeatureConfig firstFeature;
      firstFeature.id = "circle_a";
      firstFeature.polarity = "NB";
      firstFeature.searchRoi = QRect(100, 100, 100, 100);
      firstFeature.thresholdMin = 0;
      firstFeature.thresholdMax = 80;
      firstFeature.expectedRadiusMin = 20.0;
      firstFeature.expectedRadiusMax = 80.0;
      config.features.append(firstFeature);
    }
    config.features.append(feature);
  }

  return saveSurfaceLocalizationStrategy(cameraId, config, errorMessage);
}
