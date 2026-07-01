#include "RecipeManager.h"

#include "config/AppConfig.h"

#include "RecipeJsonUtils.h"

using namespace RecipeJsonUtils;

namespace
{
QJsonObject acquisitionObjectFromConfig(const CameraAcquisitionConfig& acquisition)
{
  QJsonObject acquisitionObject;
  acquisitionObject["autoExposure"] = acquisition.autoExposure;
  if (acquisition.hasExposure)
  {
    acquisitionObject["exposure"] = acquisition.exposure;
  }
  acquisitionObject["autoGain"] = acquisition.autoGain;
  if (acquisition.hasGain)
  {
    acquisitionObject["gain"] = acquisition.gain;
  }
  acquisitionObject["autoWhiteBalance"] = acquisition.autoWhiteBalance;
  if (acquisition.hasWhiteBalance)
  {
    acquisitionObject["whiteBalance"] = acquisition.whiteBalance;
  }
  if (acquisition.frameWidth > 0 && acquisition.frameHeight > 0)
  {
    acquisitionObject["frameWidth"] = acquisition.frameWidth;
    acquisitionObject["frameHeight"] = acquisition.frameHeight;
  }
  acquisitionObject["frameIntervalMs"] = acquisition.frameIntervalMs;
  return acquisitionObject;
}

void applyAcquisitionObject(const QJsonObject& acquisitionObject, CameraAcquisitionConfig& acquisition)
{
  acquisition.autoExposure = acquisitionObject.value("autoExposure").toBool(acquisition.autoExposure);
  acquisition.hasExposure = acquisitionObject.contains("exposure");
  acquisition.exposure = acquisitionObject.value("exposure").toDouble(acquisition.exposure);
  acquisition.autoGain = acquisitionObject.value("autoGain").toBool(acquisition.autoGain);
  acquisition.hasGain = acquisitionObject.contains("gain");
  acquisition.gain = acquisitionObject.value("gain").toDouble(acquisition.gain);
  acquisition.autoWhiteBalance = acquisitionObject.value("autoWhiteBalance").toBool(acquisition.autoWhiteBalance);
  acquisition.hasWhiteBalance = acquisitionObject.contains("whiteBalance");
  acquisition.whiteBalance = acquisitionObject.value("whiteBalance").toDouble(acquisition.whiteBalance);
  acquisition.frameWidth = acquisitionObject.value("frameWidth").toInt(0);
  acquisition.frameHeight = acquisitionObject.value("frameHeight").toInt(0);
  acquisition.frameIntervalMs = acquisitionObject.value("frameIntervalMs").toInt(acquisition.frameIntervalMs);
}
}

bool RecipeManager::hasCameraAcquisitionSettings(const QString& cameraId) const
{
  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return false;
  }

  return root.contains("acquisition") && root.value("acquisition").isObject();
}

bool RecipeManager::loadCameraAcquisitionSettings(const QString& cameraId, CameraAcquisitionConfig& acquisition) const
{
  QJsonObject root;
  if (!loadJsonObject(cameraRecipePath(cameraId), root))
  {
    return false;
  }

  const QJsonObject acquisitionObject = root.value("acquisition").toObject();
  if (acquisitionObject.isEmpty())
  {
    return false;
  }

  applyAcquisitionObject(acquisitionObject, acquisition);
  return true;
}

bool RecipeManager::saveCameraAcquisitionSettings(
  const QString& cameraId,
  const CameraAcquisitionConfig& acquisition,
  QString* errorMessage) const
{
  const QString path = cameraRecipePath(cameraId);
  QJsonObject root;
  if (!loadJsonObject(path, root))
  {
    root["cameraId"] = cameraId;
  }

  root["acquisition"] = acquisitionObjectFromConfig(acquisition);
  return saveJsonObject(path, root, errorMessage);
}
