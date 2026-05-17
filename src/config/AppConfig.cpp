#include "AppConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
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
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile aprire configurazione: " + filePath;
    }

    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "JSON configurazione non valido: " + parseError.errorString();
    }

    return false;
  }

  const QJsonObject root = document.object();
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
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile aprire configurazione: " + filePath;
    }

    return false;
  }

  QJsonParseError parseError;
  QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();

  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "JSON configurazione non valido: " + parseError.errorString();
    }

    return false;
  }

  QJsonObject root = document.object();
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
  document.setObject(root);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile scrivere configurazione: " + filePath;
    }

    return false;
  }

  file.write(document.toJson(QJsonDocument::Indented));
  return true;
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
