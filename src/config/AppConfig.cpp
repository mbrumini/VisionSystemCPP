#include "AppConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QJsonObject loadJsonObject(const QString& filePath, QString* errorMessage)
{
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile aprire configurazione: " + filePath;
    }

    return {};
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "JSON configurazione non valido: " + parseError.errorString();
    }

    return {};
  }

  return document.object();
}

bool writeJsonObject(const QString& filePath, const QJsonObject& root, QString* errorMessage)
{
  QFile file(filePath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile scrivere configurazione: " + filePath;
    }

    return false;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

QStringList toStringList(const QJsonArray& values)
{
  QStringList result;

  for (const QJsonValue& value : values)
  {
    result.append(value.toString());
  }

  return result;
}

QString defaultBackendForType(const QString& type)
{
  if (type == "vimba")
  {
    return "vimbax";
  }
  if (type == "usb")
  {
    return "opencv_usb";
  }
  return type;
}

QString defaultCameraProfileForBackend(const QString& backend)
{
  if (backend == "vimbax")
  {
    return "allied_vimbax_generic";
  }
  if (backend == "opencv_usb")
  {
    return "opencv_usb_generic";
  }
  if (backend == "file" || backend == "simulator")
  {
    return "file_simulated";
  }
  if (backend == "svs")
  {
    return "svs_vistek_gige_generic";
  }
  return {};
}
}

bool AppConfig::load(const QString& filePath, QString* errorMessage)
{
  const QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  m_maxCameras = root.value("system").toObject().value("maxCameras").toInt(16);
  m_cameras.clear();

  QHash<QString, ProcessingProfile> profiles;
  const QJsonObject profileRoot = root.value("processingProfiles").toObject();

  for (auto it = profileRoot.begin(); it != profileRoot.end(); ++it)
  {
    const QJsonObject profileObject = it.value().toObject();

    ProcessingProfile profile;
    profile.id = it.key();
    profile.imageMode = profileObject.value("imageMode").toString();
    profile.inspectionTypes = toStringList(profileObject.value("inspectionTypes").toArray());
    profile.guiTools = toStringList(profileObject.value("guiTools").toArray());
    profiles.insert(profile.id, profile);
  }

  const QJsonArray cameras = root.value("cameras").toArray();

  for (const QJsonValue& cameraValue : cameras)
  {
    const QJsonObject cameraObject = cameraValue.toObject();

    CameraConfig camera;
    camera.slot = cameraObject.value("slot").toInt(m_cameras.size() + 1);
    camera.id = cameraObject.value("id").toString();
    camera.displayName = cameraObject.value("displayName").toString(camera.id);
    camera.exists = cameraObject.value("exists").toBool();
    camera.enabled = cameraObject.value("enabled").toBool();
    camera.type = cameraObject.value("type").toString();
    camera.backend = cameraObject.value("backend").toString(defaultBackendForType(camera.type));
    camera.cameraProfileId = cameraObject.value("cameraProfile").toString(
      defaultCameraProfileForBackend(camera.backend));
    camera.folder = cameraObject.value("folder").toString();
    camera.usbIndex = cameraObject.value("usbIndex").toInt(-1);
    camera.serial = cameraObject.value("serial").toString();
    camera.deviceId = cameraObject.value("deviceId").toString(camera.serial);
    camera.modelName = cameraObject.value("modelName").toString();
    camera.interfaceId = cameraObject.value("interfaceId").toString();
    camera.simulatorChannel = cameraObject.value("simulatorChannel").toString(camera.id);
    const QJsonObject triggerObject = cameraObject.value("trigger").toObject();
    camera.trigger.mode = triggerObject.value("mode").toString();
    camera.trigger.source = triggerObject.value("source").toString();
    camera.trigger.ioOutput = triggerObject.value("ioOutput").toString();
    camera.trigger.cameraLine = triggerObject.value("cameraLine").toString();
    const QJsonObject calibrationObject = cameraObject.value("calibration").toObject();
    camera.calibration.enabled = calibrationObject.value("enabled").toBool(false);
    camera.calibration.type = calibrationObject.value("type").toString("none");
    camera.calibration.file = calibrationObject.value("file").toString();
    camera.calibration.pixelSizeXMm = calibrationObject.value("pixelSizeXMm").toDouble(0.0);
    camera.calibration.pixelSizeYMm = calibrationObject.value("pixelSizeYMm").toDouble(camera.calibration.pixelSizeXMm);
    camera.calibration.updatedAt = calibrationObject.value("updatedAt").toString();
    const QJsonObject acquisitionObject = cameraObject.value("acquisition").toObject();
    camera.acquisition.autoExposure = acquisitionObject.value("autoExposure").toBool(camera.acquisition.autoExposure);
    camera.acquisition.hasExposure = acquisitionObject.contains("exposure");
    camera.acquisition.exposure = acquisitionObject.value("exposure").toDouble(camera.acquisition.exposure);
    camera.acquisition.autoGain = acquisitionObject.value("autoGain").toBool(camera.acquisition.autoGain);
    camera.acquisition.hasGain = acquisitionObject.contains("gain");
    camera.acquisition.gain = acquisitionObject.value("gain").toDouble(camera.acquisition.gain);
    camera.acquisition.autoWhiteBalance = acquisitionObject.value("autoWhiteBalance").toBool(camera.acquisition.autoWhiteBalance);
    camera.acquisition.hasWhiteBalance = acquisitionObject.contains("whiteBalance");
    camera.acquisition.whiteBalance = acquisitionObject.value("whiteBalance").toDouble(camera.acquisition.whiteBalance);
    camera.acquisition.frameWidth = acquisitionObject.value("frameWidth").toInt(0);
    camera.acquisition.frameHeight = acquisitionObject.value("frameHeight").toInt(0);
    camera.acquisition.frameIntervalMs = acquisitionObject.value("frameIntervalMs").toInt(camera.acquisition.frameIntervalMs);
    camera.processingProfileId = cameraObject.value("processingProfile").toString("default");
    camera.profile = profiles.value(camera.processingProfileId, profiles.value("default"));

    m_cameras.append(camera);
  }

  return true;
}

bool AppConfig::saveCameraSource(
  const QString& filePath,
  const QString& cameraId,
  const QString& type,
  const QString& folder,
  QString* errorMessage)
{
  QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  QJsonArray cameras = root.value("cameras").toArray();
  bool found = false;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QJsonObject cameraObject = cameras[i].toObject();

    if (cameraObject.value("id").toString() != cameraId)
    {
      continue;
    }

    cameraObject["type"] = type;
    cameraObject["backend"] = defaultBackendForType(type);
    cameraObject["cameraProfile"] = defaultCameraProfileForBackend(cameraObject.value("backend").toString());
    cameraObject["folder"] = folder;
    cameraObject.remove("usbIndex");
    cameraObject.remove("serial");
    cameraObject.remove("deviceId");
    cameraObject.remove("modelName");
    cameraObject.remove("interfaceId");
    cameraObject.remove("trigger");
    cameras[i] = cameraObject;
    found = true;
    break;
  }

  if (!found)
  {
    if (errorMessage)
    {
      *errorMessage = "Camera non trovata in configurazione: " + cameraId;
    }

    return false;
  }

  root["cameras"] = cameras;
  return writeJsonObject(filePath, root, errorMessage);
}

bool AppConfig::saveVimbaCameraAssignment(
  const QString& filePath,
  int slot,
  const QString& deviceId,
  const QString& serial,
  const QString& displayName,
  const QString& modelName,
  const QString& interfaceId,
  bool enabled,
  QString* errorMessage)
{
  QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  QJsonArray cameras = root.value("cameras").toArray();
  const QString cameraId = QString("CAM%1").arg(slot, 2, 10, QChar('0'));
  bool found = false;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QJsonObject cameraObject = cameras[i].toObject();

    if (cameraObject.value("slot").toInt() != slot && cameraObject.value("id").toString() != cameraId)
    {
      continue;
    }

    cameraObject["slot"] = slot;
    cameraObject["id"] = cameraId;
    cameraObject["displayName"] = displayName.isEmpty() ? QString("Camera %1").arg(slot) : displayName;
    cameraObject["exists"] = true;
    cameraObject["enabled"] = enabled;
    cameraObject["type"] = "vimba";
    cameraObject["backend"] = "vimbax";
    cameraObject["cameraProfile"] = "allied_vimbax_generic";
    cameraObject["deviceId"] = deviceId;
    cameraObject["serial"] = serial;
    cameraObject["modelName"] = modelName;
    cameraObject["interfaceId"] = interfaceId;
    cameraObject.remove("folder");
    cameraObject.remove("usbIndex");

    QJsonObject triggerObject;
    triggerObject["mode"] = "external";
    triggerObject["source"] = "ioBoard";
    triggerObject["ioOutput"] = "";
    triggerObject["cameraLine"] = "Line1";
    cameraObject["trigger"] = triggerObject;

    if (!cameraObject.contains("processingProfile"))
    {
      cameraObject["processingProfile"] = "default";
    }

    cameras[i] = cameraObject;
    found = true;
    break;
  }

  if (!found)
  {
    QJsonObject cameraObject;
    cameraObject["slot"] = slot;
    cameraObject["id"] = cameraId;
    cameraObject["displayName"] = displayName.isEmpty() ? QString("Camera %1").arg(slot) : displayName;
    cameraObject["exists"] = true;
    cameraObject["enabled"] = enabled;
    cameraObject["type"] = "vimba";
    cameraObject["backend"] = "vimbax";
    cameraObject["cameraProfile"] = "allied_vimbax_generic";
    cameraObject["deviceId"] = deviceId;
    cameraObject["serial"] = serial;
    cameraObject["modelName"] = modelName;
    cameraObject["interfaceId"] = interfaceId;
    cameraObject["processingProfile"] = "default";

    QJsonObject triggerObject;
    triggerObject["mode"] = "external";
    triggerObject["source"] = "ioBoard";
    triggerObject["ioOutput"] = "";
    triggerObject["cameraLine"] = "Line1";
    cameraObject["trigger"] = triggerObject;
    cameras.append(cameraObject);
  }

  root["cameras"] = cameras;
  return writeJsonObject(filePath, root, errorMessage);
}

bool AppConfig::saveUsbCameraAssignment(
  const QString& filePath,
  int slot,
  int usbIndex,
  const QString& displayName,
  bool enabled,
  QString* errorMessage)
{
  QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  QJsonArray cameras = root.value("cameras").toArray();
  const QString cameraId = QString("CAM%1").arg(slot, 2, 10, QChar('0'));
  bool found = false;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QJsonObject cameraObject = cameras[i].toObject();

    if (cameraObject.value("slot").toInt() != slot && cameraObject.value("id").toString() != cameraId)
    {
      continue;
    }

    cameraObject["slot"] = slot;
    cameraObject["id"] = cameraId;
    cameraObject["displayName"] = displayName.isEmpty() ? QString("USB Camera %1").arg(usbIndex) : displayName;
    cameraObject["exists"] = true;
    cameraObject["enabled"] = enabled;
    cameraObject["type"] = "usb";
    cameraObject["backend"] = "opencv_usb";
    cameraObject["cameraProfile"] = "opencv_usb_generic";
    cameraObject["usbIndex"] = usbIndex;
    cameraObject["deviceId"] = QString("usb:%1").arg(usbIndex);
    cameraObject.remove("folder");
    cameraObject.remove("serial");
    cameraObject.remove("modelName");
    cameraObject.remove("interfaceId");

    QJsonObject triggerObject;
    triggerObject["mode"] = "software";
    triggerObject["source"] = "usbCamera";
    triggerObject["ioOutput"] = "";
    triggerObject["cameraLine"] = "";
    cameraObject["trigger"] = triggerObject;

    if (!cameraObject.contains("processingProfile"))
    {
      cameraObject["processingProfile"] = "default";
    }

    cameras[i] = cameraObject;
    found = true;
    break;
  }

  if (!found)
  {
    QJsonObject cameraObject;
    cameraObject["slot"] = slot;
    cameraObject["id"] = cameraId;
    cameraObject["displayName"] = displayName.isEmpty() ? QString("USB Camera %1").arg(usbIndex) : displayName;
    cameraObject["exists"] = true;
    cameraObject["enabled"] = enabled;
    cameraObject["type"] = "usb";
    cameraObject["backend"] = "opencv_usb";
    cameraObject["cameraProfile"] = "opencv_usb_generic";
    cameraObject["usbIndex"] = usbIndex;
    cameraObject["deviceId"] = QString("usb:%1").arg(usbIndex);
    cameraObject["processingProfile"] = "default";

    QJsonObject triggerObject;
    triggerObject["mode"] = "software";
    triggerObject["source"] = "usbCamera";
    triggerObject["ioOutput"] = "";
    triggerObject["cameraLine"] = "";
    cameraObject["trigger"] = triggerObject;
    cameras.append(cameraObject);
  }

  root["cameras"] = cameras;
  return writeJsonObject(filePath, root, errorMessage);
}

bool AppConfig::saveCameraAcquisitionSettings(
  const QString& filePath,
  const QString& cameraId,
  const CameraAcquisitionConfig& acquisition,
  QString* errorMessage)
{
  QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  QJsonArray cameras = root.value("cameras").toArray();
  bool found = false;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QJsonObject cameraObject = cameras[i].toObject();
    if (cameraObject.value("id").toString() != cameraId)
    {
      continue;
    }

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
    cameraObject["acquisition"] = acquisitionObject;
    cameras[i] = cameraObject;
    found = true;
    break;
  }

  if (!found)
  {
    if (errorMessage)
    {
      *errorMessage = "Camera non trovata in configurazione: " + cameraId;
    }
    return false;
  }

  root["cameras"] = cameras;
  if (!writeJsonObject(filePath, root, errorMessage))
  {
    return false;
  }
  updateCameraAcquisitionSettings(cameraId, acquisition);
  return true;
}

bool AppConfig::updateCameraAcquisitionSettings(
  const QString& cameraId,
  const CameraAcquisitionConfig& acquisition)
{
  for (CameraConfig& camera : m_cameras)
  {
    if (camera.id == cameraId)
    {
      camera.acquisition = acquisition;
      return true;
    }
  }
  return false;
}

bool AppConfig::saveCameraSystemSettings(
  const QString& filePath,
  const QVector<CameraConfig>& updatedCameras,
  QString* errorMessage)
{
  QJsonObject root = loadJsonObject(filePath, errorMessage);
  if (root.isEmpty())
  {
    return false;
  }

  QJsonArray cameras = root.value("cameras").toArray();
  QHash<QString, CameraConfig> byId;
  QHash<int, CameraConfig> bySlot;
  for (const CameraConfig& camera : updatedCameras)
  {
    byId.insert(camera.id, camera);
    bySlot.insert(camera.slot, camera);
  }

  for (int i = 0; i < cameras.size(); ++i)
  {
    QJsonObject cameraObject = cameras[i].toObject();
    const QString id = cameraObject.value("id").toString();
    const int slot = cameraObject.value("slot").toInt(i + 1);
    const CameraConfig camera = byId.contains(id) ? byId.value(id) : bySlot.value(slot);
    if (camera.id.isEmpty())
    {
      continue;
    }

    cameraObject["slot"] = camera.slot;
    cameraObject["id"] = camera.id;
    cameraObject["displayName"] = camera.displayName;
    cameraObject["exists"] = camera.exists;
    cameraObject["enabled"] = camera.enabled;
    if (!camera.type.isEmpty())
    {
      cameraObject["type"] = camera.type;
    }
    const QString backend = camera.backend.isEmpty() ? defaultBackendForType(camera.type) : camera.backend;
    if (!backend.isEmpty())
    {
      cameraObject["backend"] = backend;
    }
    const QString cameraProfile = camera.cameraProfileId.isEmpty()
      ? defaultCameraProfileForBackend(backend)
      : camera.cameraProfileId;
    if (!cameraProfile.isEmpty())
    {
      cameraObject["cameraProfile"] = cameraProfile;
    }
    if (camera.type == "simulator")
    {
      cameraObject["simulatorChannel"] = camera.simulatorChannel.isEmpty()
        ? camera.id
        : camera.simulatorChannel;
      cameraObject.remove("folder");
      cameraObject.remove("usbIndex");
      cameraObject.remove("serial");
      cameraObject.remove("deviceId");
      cameraObject.remove("modelName");
      cameraObject.remove("interfaceId");
      cameraObject.remove("trigger");
    }
    else
    {
      cameraObject.remove("simulatorChannel");
    }
    cameraObject["processingProfile"] = camera.processingProfileId.isEmpty()
      ? QString("default")
      : camera.processingProfileId;
    QJsonObject calibrationObject;
    calibrationObject["enabled"] = camera.calibration.enabled;
    calibrationObject["type"] = camera.calibration.type.isEmpty() ? QString("none") : camera.calibration.type;
    calibrationObject["file"] = camera.calibration.file;
    calibrationObject["pixelSizeXMm"] = camera.calibration.pixelSizeXMm;
    calibrationObject["pixelSizeYMm"] = camera.calibration.pixelSizeYMm;
    calibrationObject["updatedAt"] = camera.calibration.updatedAt;
    cameraObject["calibration"] = calibrationObject;
    QJsonObject acquisitionObject;
    acquisitionObject["autoExposure"] = camera.acquisition.autoExposure;
    if (camera.acquisition.hasExposure)
    {
      acquisitionObject["exposure"] = camera.acquisition.exposure;
    }
    acquisitionObject["autoGain"] = camera.acquisition.autoGain;
    if (camera.acquisition.hasGain)
    {
      acquisitionObject["gain"] = camera.acquisition.gain;
    }
    acquisitionObject["autoWhiteBalance"] = camera.acquisition.autoWhiteBalance;
    if (camera.acquisition.hasWhiteBalance)
    {
      acquisitionObject["whiteBalance"] = camera.acquisition.whiteBalance;
    }
    if (camera.acquisition.frameWidth > 0 && camera.acquisition.frameHeight > 0)
    {
      acquisitionObject["frameWidth"] = camera.acquisition.frameWidth;
      acquisitionObject["frameHeight"] = camera.acquisition.frameHeight;
    }
    acquisitionObject["frameIntervalMs"] = camera.acquisition.frameIntervalMs;
    cameraObject["acquisition"] = acquisitionObject;
    if (!camera.modelName.isEmpty())
    {
      cameraObject["modelName"] = camera.modelName;
    }
    if (!camera.interfaceId.isEmpty())
    {
      cameraObject["interfaceId"] = camera.interfaceId;
    }
    cameras[i] = cameraObject;
  }

  root["cameras"] = cameras;
  return writeJsonObject(filePath, root, errorMessage);
}

int AppConfig::maxCameras() const
{
  return m_maxCameras;
}

const QVector<CameraConfig>& AppConfig::cameras() const
{
  return m_cameras;
}

QVector<CameraConfig> AppConfig::activeCameras() const
{
  QVector<CameraConfig> result;

  for (const CameraConfig& camera : m_cameras)
  {
    if (camera.exists && camera.enabled)
    {
      result.append(camera);
    }
  }

  return result;
}
