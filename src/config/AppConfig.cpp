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
    camera.folder = cameraObject.value("folder").toString();
    camera.usbIndex = cameraObject.value("usbIndex").toInt(-1);
    camera.serial = cameraObject.value("serial").toString();
    camera.deviceId = cameraObject.value("deviceId").toString(camera.serial);
    camera.modelName = cameraObject.value("modelName").toString();
    camera.interfaceId = cameraObject.value("interfaceId").toString();
    const QJsonObject triggerObject = cameraObject.value("trigger").toObject();
    camera.trigger.mode = triggerObject.value("mode").toString();
    camera.trigger.source = triggerObject.value("source").toString();
    camera.trigger.ioOutput = triggerObject.value("ioOutput").toString();
    camera.trigger.cameraLine = triggerObject.value("cameraLine").toString();
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
    cameraObject["displayName"] = displayName.isEmpty() ? QString("Camera %1").arg(slot) : displayName;
    cameraObject["exists"] = true;
    cameraObject["enabled"] = enabled;
    cameraObject["type"] = "vimba";
    cameraObject["deviceId"] = deviceId;
    cameraObject["serial"] = serial;
    cameraObject["modelName"] = modelName;
    cameraObject["interfaceId"] = interfaceId;
    cameraObject["processingProfile"] = "default";

    QJsonObject triggerObject;
    triggerObject["mode"] = "external";
    triggerObject["source"] = "ioBoard";
    triggerObject["ioOutput"] = "";
    triggerObject["cameraLine"] = "";
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
