#include "RecipeManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPolygon>

#include "RecipeJsonUtils.h"

using namespace RecipeJsonUtils;

bool RecipeManager::loadSurfaceDefectRoi(const QString& cameraId, QRect& roi) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return false;
  }

  const QJsonObject surfaceLocalization = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  const QJsonObject methodAoe = surfaceLocalization.value("aoes").toObject()
    .value(method).toObject();
  QJsonObject searchRoi = methodAoe.value("searchRoi").toObject();
  if (searchRoi.isEmpty())
  {
    searchRoi = surfaceLocalization.value("searchRoi").toObject();
  }

  if (searchRoi.isEmpty())
  {
    return false;
  }

  roi = rectFromJson(searchRoi);
  return roi.isValid();
}

QVector<QPoint> RecipeManager::loadSurfaceDefectPolygon(const QString& cameraId) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return {};
  }

  const QJsonObject surfaceLocalization = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  const QJsonObject methodAoe = surfaceLocalization.value("aoes").toObject()
    .value(method).toObject();
  QJsonArray polygon = methodAoe.value("searchPolygon").toArray();
  if (polygon.isEmpty())
  {
    polygon = surfaceLocalization.value("searchPolygon").toArray();
  }

  QVector<QPoint> result;
  result.reserve(polygon.size());
  for (const QJsonValue& value : polygon)
  {
    result.append(pointFromJson(value.toObject()));
  }

  return result.size() >= 3 ? result : QVector<QPoint>();
}

SurfaceDefectSettings RecipeManager::loadSurfaceDefectSettings(const QString& cameraId) const
{
  SurfaceDefectSettings settings;
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return settings;
  }

  const QJsonObject threshold = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("threshold").toObject();

  if (threshold.isEmpty())
  {
    return settings;
  }

  settings.thresholdMin = threshold.value("min").toInt(settings.thresholdMin);
  settings.thresholdMax = threshold.value("max").toInt(settings.thresholdMax);
  settings.pcaResolveAmbiguity = threshold.value("pcaResolveAmbiguity").toBool(settings.pcaResolveAmbiguity);
  return settings;
}

QVector<QRect> RecipeManager::loadSurfaceDefectExclusionRects(const QString& cameraId) const
{
  QJsonObject root;

  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return {};
  }

  const QJsonArray exclusions = root.value("tools").toObject()
    .value("surfaceLocalization").toObject()
    .value("exclusionRects").toArray();
  QVector<QRect> result;

  for (const QJsonValue& value : exclusions)
  {
    const QRect rect = rectFromJson(value.toObject()).normalized();

    if (rect.isValid())
    {
      result.append(rect);
    }
  }

  return result;
}

bool RecipeManager::saveSurfaceDefectRoi(const QString& cameraId, const QRect& roi, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceDefects.value("method").toString("threshold"));
  pruneSurfaceLocalizationForMethod(surfaceDefects, method);

  QJsonObject aoes = surfaceDefects.value("aoes").toObject();
  QJsonObject methodAoe = aoes.value(method).toObject();
  methodAoe["id"] = method;
  methodAoe["label"] = method;
  methodAoe["searchRoi"] = rectToJson(roi.normalized());
  methodAoe.remove("searchPolygon");
  aoes[method] = methodAoe;
  surfaceDefects["aoes"] = aoes;

  if (method == "threshold" || method == "massPca")
  {
    QJsonObject threshold = surfaceDefects.value("threshold").toObject();

    if (!threshold.contains("min"))
    {
      threshold["min"] = SurfaceDefectSettings().thresholdMin;
    }

    if (!threshold.contains("max"))
    {
      threshold["max"] = SurfaceDefectSettings().thresholdMax;
    }

    surfaceDefects["threshold"] = threshold;
  }
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceDefectPolygon(const QString& cameraId, const QVector<QPoint>& polygon, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  pruneSurfaceLocalizationForMethod(surfaceLocalization, method);

  QJsonArray points;
  for (const QPoint& point : polygon)
  {
    points.append(pointToJson(point));
  }

  if (polygon.size() >= 3)
  {
    const QRect boundingRoi = QPolygon(polygon).boundingRect().normalized();

    QJsonObject aoes = surfaceLocalization.value("aoes").toObject();
    QJsonObject methodAoe = aoes.value(method).toObject();
    methodAoe["id"] = method;
    methodAoe["label"] = method;
    methodAoe["searchRoi"] = rectToJson(boundingRoi);
    methodAoe["searchPolygon"] = points;
    aoes[method] = methodAoe;
    surfaceLocalization["aoes"] = aoes;
  }

  if (method == "threshold" || method == "massPca")
  {
    QJsonObject threshold = surfaceLocalization.value("threshold").toObject();

    if (!threshold.contains("min"))
    {
      threshold["min"] = SurfaceDefectSettings().thresholdMin;
    }

    if (!threshold.contains("max"))
    {
      threshold["max"] = SurfaceDefectSettings().thresholdMax;
    }

    surfaceLocalization["threshold"] = threshold;
  }

  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearSurfaceDefectRoi(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  surfaceLocalization.remove("searchRoi");
  QJsonObject aoes = surfaceLocalization.value("aoes").toObject();
  QJsonObject methodAoe = aoes.value(method).toObject();
  methodAoe.remove("searchRoi");
  aoes[method] = methodAoe;
  surfaceLocalization["aoes"] = aoes;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearSurfaceDefectPolygon(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  surfaceLocalization.remove("searchPolygon");
  QJsonObject aoes = surfaceLocalization.value("aoes").toObject();
  QJsonObject methodAoe = aoes.value(method).toObject();
  methodAoe.remove("searchPolygon");
  aoes[method] = methodAoe;
  surfaceLocalization["aoes"] = aoes;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceDefectThreshold(const QString& cameraId, int minValue, int maxValue, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("massPca"));
  pruneSurfaceLocalizationForMethod(surfaceLocalization, method == "threshold" ? "threshold" : "massPca");

  QJsonObject threshold = surfaceLocalization.value("threshold").toObject();
  threshold["min"] = qBound(0, minValue, 255);
  threshold["max"] = qBound(threshold.value("min").toInt(), maxValue, 255);
  threshold["value"] = threshold.value("max").toInt();
  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::addSurfaceDefectExclusionRect(const QString& cameraId, const QRect& rect, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceDefects, surfaceDefects.value("method").toString("threshold"));

  QJsonArray exclusions = surfaceDefects.value("exclusionRects").toArray();
  exclusions.append(rectToJson(rect.normalized()));
  surfaceDefects["exclusionRects"] = exclusions;
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceDefectExclusionRects(const QString& cameraId, const QVector<QRect>& rects, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["enabled"] = true;
  pruneSurfaceLocalizationForMethod(surfaceDefects, surfaceDefects.value("method").toString("threshold"));

  QJsonArray exclusions;
  for (const QRect& rect : rects)
  {
    const QRect normalized = rect.normalized();
    if (normalized.isValid())
    {
      exclusions.append(rectToJson(normalized));
    }
  }

  surfaceDefects["exclusionRects"] = exclusions;
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearSurfaceDefectExclusionRects(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceDefects = tools.value("surfaceLocalization").toObject();
  surfaceDefects["exclusionRects"] = QJsonArray();
  tools["surfaceLocalization"] = surfaceDefects;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveSurfaceDefectPcaResolveAmbiguity(const QString& cameraId, bool resolve, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["enabled"] = true;
  const QString method = normalizedSurfaceLocalizationMethod(surfaceLocalization.value("method").toString("threshold"));
  pruneSurfaceLocalizationForMethod(surfaceLocalization, method == "threshold" ? "threshold" : "massPca");

  QJsonObject threshold = surfaceLocalization.value("threshold").toObject();
  threshold["pcaResolveAmbiguity"] = resolve;
  surfaceLocalization["threshold"] = threshold;
  tools["surfaceLocalization"] = surfaceLocalization;
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::loadGlobalSurfaceAoi(const QString& cameraId, QRect& roi) const
{
  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return false;
  }
  const QJsonObject surfaceLocalization = root.value("tools").toObject()
    .value("surfaceLocalization").toObject();
  const QJsonObject searchRoi = surfaceLocalization.value("searchRoi").toObject();
  if (searchRoi.isEmpty())
  {
    return false;
  }
  roi = rectFromJson(searchRoi);
  return roi.isValid();
}

bool RecipeManager::saveGlobalSurfaceAoi(const QString& cameraId, const QRect& roi, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();

  // 1. Save to surfaceLocalization.searchRoi
  QJsonObject surfaceLocalization = tools.value("surfaceLocalization").toObject();
  surfaceLocalization["searchRoi"] = rectToJson(roi.normalized());
  tools["surfaceLocalization"] = surfaceLocalization;

  // 2. Save to localization.searchRoi for consistency
  QJsonObject localization = tools.value("localization").toObject();
  localization["searchRoi"] = rectToJson(roi.normalized());
  tools["localization"] = localization;

  root["tools"] = tools;
  return saveJsonObject(path, root, errorMessage);
}

