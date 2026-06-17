#include "RecipeManager.h"

#include "RecipeJsonUtils.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace RecipeJsonUtils;

QVector<MeasurementRecipeConfig> RecipeManager::loadMeasurements(const QString& cameraId) const
{
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
    config.enabled = item.value("enabled").toBool(config.enabled);
    config.type = item.value("type").toString();
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
  for (const MeasurementRecipeConfig& config : configs)
  {
    if (config.type.isEmpty() || config.sourceAId.isEmpty())
    {
      continue;
    }

    QJsonObject item;
    item["enabled"] = config.enabled;
    item["id"] = config.id.isEmpty() ? QString("measurement_%1").arg(items.size() + 1) : config.id;
    item["type"] = config.type;
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
  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::appendMeasurement(const QString& cameraId, const MeasurementRecipeConfig& config, QString* errorMessage) const
{
  QVector<MeasurementRecipeConfig> configs = loadMeasurements(cameraId);
  for (MeasurementRecipeConfig& existing : configs)
  {
    if (existing.type == config.type &&
        existing.sourceAId == config.sourceAId &&
        existing.sourceBId == config.sourceBId)
    {
      existing.enabled = config.enabled;
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
    saved.id = QString("measurement_%1").arg(configs.size() + 1);
  }
  configs.append(saved);
  return saveMeasurements(cameraId, configs, errorMessage);
}
