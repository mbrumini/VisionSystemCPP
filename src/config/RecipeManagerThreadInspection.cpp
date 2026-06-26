#include "RecipeManager.h"

#include "config/GeometryRecipeJson.h"
#include "runtime/PartPose.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "RecipeJsonUtils.h"

using namespace RecipeJsonUtils;

namespace
{
QJsonObject threadFilterToJson(const ThreadInspectionSettings& settings)
{
  QJsonObject filter;
  filter["thresholdMin"] = settings.thresholdMin;
  filter["thresholdMax"] = settings.thresholdMax;
  filter["morphOpenRadius"] = settings.morphOpenRadius;
  filter["minSpeckAreaPx"] = settings.minSpeckAreaPx;
  filter["maxSpeckAreaPx"] = settings.maxSpeckAreaPx;
  filter["profileSmoothRadius"] = settings.profileSmoothRadius;
  filter["outlierRejectSigma"] = settings.outlierRejectSigma;
  return filter;
}

void applyThreadFilterFromJson(ThreadInspectionSettings& settings, const QJsonObject& filter)
{
  settings.thresholdMin = filter.value("thresholdMin").toInt(settings.thresholdMin);
  settings.thresholdMax = filter.value("thresholdMax").toInt(settings.thresholdMax);
  settings.morphOpenRadius = filter.value("morphOpenRadius").toInt(settings.morphOpenRadius);
  settings.minSpeckAreaPx = filter.value("minSpeckAreaPx").toInt(settings.minSpeckAreaPx);
  settings.maxSpeckAreaPx = filter.value("maxSpeckAreaPx").toInt(settings.maxSpeckAreaPx);
  settings.profileSmoothRadius = filter.value("profileSmoothRadius").toInt(settings.profileSmoothRadius);
  settings.outlierRejectSigma = filter.value("outlierRejectSigma").toDouble(settings.outlierRejectSigma);
}

void applyThreadMeasurementLimitsFromJson(ThreadMeasurementLimits& limits, const QJsonObject& object)
{
  limits.enabled = object.value("enabled").toBool(limits.enabled);
  limits.alias = object.value("alias").toString(limits.alias);
  limits.nominal = object.value("nominal").toDouble(limits.nominal);
  limits.min = object.value("min").toDouble(limits.min);
  limits.max = object.value("max").toDouble(limits.max);
  limits.hasNominal = object.value("hasNominal").toBool(limits.hasNominal);
  limits.hasMin = object.value("hasMin").toBool(limits.hasMin);
  limits.hasMax = object.value("hasMax").toBool(limits.hasMax);
}

QJsonObject threadMeasurementLimitsToJson(const ThreadMeasurementLimits& limits)
{
  QJsonObject object;
  if (limits.enabled)
  {
    object["enabled"] = true;
  }
  if (!limits.alias.isEmpty())
  {
    object["alias"] = limits.alias;
  }
  if (limits.hasNominal)
  {
    object["nominal"] = limits.nominal;
    object["hasNominal"] = true;
  }
  if (limits.hasMin)
  {
    object["min"] = limits.min;
    object["hasMin"] = true;
  }
  if (limits.hasMax)
  {
    object["max"] = limits.max;
    object["hasMax"] = true;
  }
  return object;
}

QJsonObject threadInspectionObjectFromSettings(const ThreadInspectionSettings& settings)
{
  QJsonObject threadInspection;
  threadInspection["enabled"] = settings.enabled;
  threadInspection["coordinateSpace"] = GeometryRecipeJson::normalizedCoordinateSpace(settings.coordinateSpace);
  if (settings.hasExtractionRoi)
  {
    QJsonObject extractionRoi;
    if (!settings.partRoi.isEmpty())
    {
      extractionRoi["part"] = rectToJson(settings.partRoi.normalized());
    }
    if (!settings.imageRoi.isEmpty())
    {
      extractionRoi["image"] = rectToJson(settings.imageRoi.normalized());
    }
    threadInspection["extractionRoi"] = extractionRoi;
  }
  threadInspection["filter"] = threadFilterToJson(settings);
  QJsonObject measurements;
  measurements["major"] = threadMeasurementLimitsToJson(settings.majorDiameter);
  measurements["pitch"] = threadMeasurementLimitsToJson(settings.pitchLength);
  measurements["minor"] = threadMeasurementLimitsToJson(settings.minorDiameter);
  measurements["phase"] = threadMeasurementLimitsToJson(settings.phaseOffset);
  threadInspection["measurements"] = measurements;
  return threadInspection;
}

ThreadInspectionSettings threadInspectionSettingsFromObject(const QJsonObject& threadInspection)
{
  ThreadInspectionSettings settings;
  settings.enabled = threadInspection.value("enabled").toBool(settings.enabled);
  settings.coordinateSpace =
    GeometryRecipeJson::normalizedCoordinateSpace(threadInspection.value("coordinateSpace").toString(settings.coordinateSpace));

  const QJsonObject extractionRoi = threadInspection.value("extractionRoi").toObject();
  const QRect partRoi = rectFromJson(extractionRoi.value("part").toObject());
  const QRect imageRoi = rectFromJson(extractionRoi.value("image").toObject());
  settings.partRoi = partRoi.normalized();
  settings.imageRoi = imageRoi.normalized();
  settings.hasExtractionRoi = settings.partRoi.isValid() || settings.imageRoi.isValid();

  applyThreadFilterFromJson(settings, threadInspection.value("filter").toObject());
  const QJsonObject measurements = threadInspection.value("measurements").toObject();
  applyThreadMeasurementLimitsFromJson(settings.majorDiameter, measurements.value("major").toObject());
  applyThreadMeasurementLimitsFromJson(settings.pitchLength, measurements.value("pitch").toObject());
  applyThreadMeasurementLimitsFromJson(settings.minorDiameter, measurements.value("minor").toObject());
  applyThreadMeasurementLimitsFromJson(settings.phaseOffset, measurements.value("phase").toObject());
  return settings;
}
}

ThreadInspectionSettings RecipeManager::loadThreadInspectionSettings(const QString& cameraId) const
{
  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return {};
  }

  const QJsonObject threadInspection = root.value("tools").toObject().value("threadInspection").toObject();
  if (threadInspection.isEmpty())
  {
    return {};
  }

  return threadInspectionSettingsFromObject(threadInspection);
}

bool RecipeManager::saveThreadInspectionSettings(
  const QString& cameraId,
  const ThreadInspectionSettings& settings,
  QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  QJsonObject tools = root.value("tools").toObject();
  ThreadInspectionSettings merged = loadThreadInspectionSettings(cameraId);
  merged.enabled = true;
  merged.thresholdMin = settings.thresholdMin;
  merged.thresholdMax = settings.thresholdMax;
  merged.morphOpenRadius = settings.morphOpenRadius;
  merged.minSpeckAreaPx = settings.minSpeckAreaPx;
  merged.maxSpeckAreaPx = settings.maxSpeckAreaPx;
  merged.profileSmoothRadius = settings.profileSmoothRadius;
  merged.outlierRejectSigma = settings.outlierRejectSigma;
  merged.majorDiameter = settings.majorDiameter;
  merged.pitchLength = settings.pitchLength;
  merged.minorDiameter = settings.minorDiameter;
  merged.phaseOffset = settings.phaseOffset;
  tools["threadInspection"] = threadInspectionObjectFromSettings(merged);
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::saveThreadInspectionExtractionRoi(
  const QString& cameraId,
  const QString& coordinateSpace,
  const QRect& partRoi,
  const QRect& imageRoi,
  QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  loadJsonObject(path, root);
  root["cameraId"] = cameraId;

  ThreadInspectionSettings settings = loadThreadInspectionSettings(cameraId);
  settings.enabled = true;
  settings.coordinateSpace = GeometryRecipeJson::normalizedCoordinateSpace(coordinateSpace);
  settings.partRoi = partRoi.normalized();
  settings.imageRoi = imageRoi.normalized();
  settings.hasExtractionRoi = settings.partRoi.isValid() || settings.imageRoi.isValid();

  QJsonObject tools = root.value("tools").toObject();
  tools["threadInspection"] = threadInspectionObjectFromSettings(settings);
  root["tools"] = tools;

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::clearThreadInspectionExtractionRoi(const QString& cameraId, QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  if (!loadJsonObject(path, root))
  {
    root["cameraId"] = cameraId;
  }

  QJsonObject tools = root.value("tools").toObject();
  QJsonObject threadInspection = tools.value("threadInspection").toObject();
  threadInspection.remove("extractionRoi");
  if (!threadInspection.isEmpty())
  {
    tools["threadInspection"] = threadInspection;
    root["tools"] = tools;
  }

  return saveJsonObject(path, root, errorMessage);
}

bool RecipeManager::migrateThreadInspectionRoiToPartSpace(
  const QString& cameraId,
  const PartPose& pose,
  QString* errorMessage) const
{
  if (!pose.valid)
  {
    return false;
  }

  ThreadInspectionSettings settings = loadThreadInspectionSettings(cameraId);
  if (!settings.hasExtractionRoi || !GeometryRecipeJson::isImageSpace(settings.coordinateSpace))
  {
    return false;
  }

  if (!settings.imageRoi.isValid())
  {
    return false;
  }

  const QRect partRoi = imageRectToPartRect(pose, settings.imageRoi.normalized()).normalized();
  if (!partRoi.isValid())
  {
    return false;
  }

  return saveThreadInspectionExtractionRoi(
    cameraId,
    GeometryRecipeJson::kPartSpace,
    partRoi,
    settings.imageRoi.normalized(),
    errorMessage);
}
