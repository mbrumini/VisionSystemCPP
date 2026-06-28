#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace RecipeJsonUtils;

namespace
{
QString normalizeMeasurementCriterion(const QString& criterion)
{
  if (criterion == "min" || criterion == "minimum")
  {
    return "min";
  }
  if (criterion == "max" || criterion == "maximum")
  {
    return "max";
  }
  return "average";
}
}

QVector<MeasurementRecipeConfig> RecipeManager::loadMeasurements(const QString& cameraId) const
{
  const auto cached = m_measurementsCache.constFind(cameraId);
  if (cached != m_measurementsCache.constEnd())
  {
    return cached.value();
  }

  QVector<MeasurementRecipeConfig> configs;

  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return configs;
  }

  const QJsonArray measurements = root.value("tools").toObject()
    .value("measurements").toObject()
    .value("items").toArray();

  for (const QJsonValue& value : measurements)
  {
    const QJsonObject item = value.toObject();
    MeasurementRecipeConfig config;
    config.id = item.value("id").toString(QString("measurement_%1").arg(configs.size() + 1));
    config.alias = item.value("alias").toString();
    config.enabled = item.value("enabled").toBool(config.enabled);
    config.type = item.value("type").toString();
    config.criterion = normalizeMeasurementCriterion(item.value("criterion").toString(config.criterion));
    if (config.type == "line_line_distance_min")
    {
      config.type = "line_line_distance";
      config.criterion = "min";
    }
    else if (config.type == "line_line_distance_max")
    {
      config.type = "line_line_distance";
      config.criterion = "max";
    }
    config.sourceAId = item.value("sourceAId").toString();
    config.sourceBId = item.value("sourceBId").toString();
    config.unit = item.value("unit").toString(config.unit);
    config.samplePixels = item.value("samplePixels").toDouble(config.samplePixels);
    config.sampleValue = item.value("sampleValue").toDouble(config.sampleValue);
    config.hasSampleScale = item.value("hasSampleScale").toBool(config.hasSampleScale);
    config.nominal = item.value("nominal").toDouble(config.nominal);
    config.min = item.value("min").toDouble(config.min);
    config.max = item.value("max").toDouble(config.max);
    config.hasNominal = item.value("hasNominal").toBool(config.hasNominal);
    config.hasMin = item.value("hasMin").toBool(config.hasMin);
    config.hasMax = item.value("hasMax").toBool(config.hasMax);
    if (!config.hasMin && !config.hasMax && config.max > config.min)
    {
      config.hasNominal = true;
      config.hasMin = true;
      config.hasMax = true;
    }
    const QJsonObject label = item.value("label").toObject();
    if (label.value("custom").toBool(false))
    {
      config.labelPoint = QPointF(label.value("x").toDouble(), label.value("y").toDouble());
      config.hasLabelPoint = true;
    }
    if (config.id.isEmpty() || config.type.isEmpty() || config.sourceAId.isEmpty())
    {
      continue;
    }
    configs.append(config);
  }

  m_measurementsCache.insert(cameraId, configs);
  return configs;
}

bool RecipeManager::saveMeasurements(const QString& cameraId, const QVector<MeasurementRecipeConfig>& configs, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);

  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject measurements;
  QJsonArray items;
  QStringList assignedIds;
  for (const MeasurementRecipeConfig& config : configs)
  {
    if (config.type.isEmpty() || config.sourceAId.isEmpty())
    {
      continue;
    }

    QJsonObject item;
    item["enabled"] = config.enabled;
    item["id"] = ensureUniquePrefixedId("measurement", config.id, assignedIds);
    if (!config.alias.trimmed().isEmpty())
    {
      item["alias"] = config.alias.trimmed();
    }
    item["type"] = config.type;
    item["criterion"] = normalizeMeasurementCriterion(config.criterion);
    item["sourceAId"] = config.sourceAId;
    item["unit"] = config.unit;
    item["samplePixels"] = config.samplePixels;
    item["sampleValue"] = config.sampleValue;
    item["hasSampleScale"] = config.hasSampleScale;
    item["nominal"] = config.nominal;
    item["min"] = config.min;
    item["max"] = config.max;
    item["hasNominal"] = config.hasNominal;
    item["hasMin"] = config.hasMin;
    item["hasMax"] = config.hasMax;
    if (!config.sourceBId.isEmpty())
    {
      item["sourceBId"] = config.sourceBId;
    }
    if (config.hasLabelPoint)
    {
      QJsonObject label;
      label["custom"] = true;
      label["x"] = config.labelPoint.x();
      label["y"] = config.labelPoint.y();
      item["label"] = label;
    }
    items.append(item);
  }

  if (items.isEmpty())
  {
    tools.remove("measurements");
  }
  else
  {
    measurements["enabled"] = true;
    measurements["items"] = items;
    tools["measurements"] = measurements;
  }

  root["tools"] = tools;
  const bool saved = saveJsonObject(path, root, errorMessage);
  if (saved)
  {
    invalidateCameraRecipeCache(cameraId);
  }
  return saved;
}

bool RecipeManager::appendMeasurement(const QString& cameraId, const MeasurementRecipeConfig& config, QString* errorMessage) const
{
  QVector<MeasurementRecipeConfig> configs = loadMeasurements(cameraId);
  for (MeasurementRecipeConfig& existing : configs)
  {
    if (existing.type == config.type &&
        normalizeMeasurementCriterion(existing.criterion) == normalizeMeasurementCriterion(config.criterion) &&
        existing.sourceAId == config.sourceAId &&
        existing.sourceBId == config.sourceBId)
    {
      existing.enabled = config.enabled;
      if (!config.alias.trimmed().isEmpty())
      {
        existing.alias = config.alias.trimmed();
      }
      if (config.hasLabelPoint)
      {
        existing.labelPoint = config.labelPoint;
        existing.hasLabelPoint = true;
      }
      if (config.samplePixels > 0.0)
      {
        existing.samplePixels = config.samplePixels;
      }
      return saveMeasurements(cameraId, configs, errorMessage);
    }
  }

  MeasurementRecipeConfig saved = config;
  if (saved.id.isEmpty())
  {
    QStringList existingIds;
    existingIds.reserve(configs.size());
    for (const MeasurementRecipeConfig& existing : configs)
    {
      existingIds.append(existing.id);
    }
    saved.id = nextPrefixedId("measurement", existingIds);
  }
  configs.append(saved);
  return saveMeasurements(cameraId, configs, errorMessage);
}
