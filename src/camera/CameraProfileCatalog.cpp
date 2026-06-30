#include "camera/CameraProfileCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
bool readOptionalDouble(const QJsonObject& object, const QString& key, double& value)
{
  if (!object.contains(key))
  {
    return false;
  }

  value = object.value(key).toDouble();
  return true;
}
}

bool CameraProfileCatalog::load(const QString& filePath, QString* errorMessage)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
    {
      *errorMessage = "Impossibile aprire profili camera: " + filePath;
    }
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    if (errorMessage)
    {
      *errorMessage = "JSON profili camera non valido: " + parseError.errorString();
    }
    return false;
  }

  m_profiles.clear();
  const QJsonArray profiles = document.object().value("profiles").toArray();
  for (const QJsonValue& value : profiles)
  {
    const QJsonObject object = value.toObject();
    CameraDriverProfile profile;
    profile.id = object.value("id").toString();
    if (profile.id.isEmpty())
    {
      continue;
    }

    profile.backend = object.value("backend").toString();
    profile.vendor = object.value("vendor").toString();
    profile.family = object.value("family").toString();
    profile.transport = object.value("transport").toString();

    const QJsonObject match = object.value("match").toObject();
    profile.vendorContains = match.value("vendorContains").toString();
    profile.modelContains = match.value("modelContains").toString();
    profile.interfaceContains = match.value("interfaceContains").toString();

    const QJsonObject limits = object.value("limits").toObject();
    profile.limits.hasExposureUsMin = readOptionalDouble(limits, "exposureUsMin", profile.limits.exposureUsMin);
    profile.limits.hasExposureUsMax = readOptionalDouble(limits, "exposureUsMax", profile.limits.exposureUsMax);
    profile.limits.hasGainMin = readOptionalDouble(limits, "gainMin", profile.limits.gainMin);
    profile.limits.hasGainMax = readOptionalDouble(limits, "gainMax", profile.limits.gainMax);

    const QJsonObject features = object.value("features").toObject();
    profile.features.softwareTrigger = features.value("softwareTrigger").toBool(profile.features.softwareTrigger);
    profile.features.hardwareTrigger = features.value("hardwareTrigger").toBool(profile.features.hardwareTrigger);
    profile.features.continuousAcquisition = features.value("continuousAcquisition").toBool(profile.features.continuousAcquisition);
    profile.features.packetSize = features.value("packetSize").toBool(profile.features.packetSize);
    profile.features.packetDelay = features.value("packetDelay").toBool(profile.features.packetDelay);

    const QJsonObject commands = object.value("commands").toObject();
    for (auto it = commands.begin(); it != commands.end(); ++it)
    {
      profile.commands.insert(it.key(), it.value().toString());
    }

    m_profiles.insert(profile.id, profile);
  }

  return true;
}

bool CameraProfileCatalog::isEmpty() const
{
  return m_profiles.isEmpty();
}

const CameraDriverProfile* CameraProfileCatalog::profile(const QString& id) const
{
  const auto it = m_profiles.constFind(id);
  return it == m_profiles.constEnd() ? nullptr : &it.value();
}

QStringList CameraProfileCatalog::ids() const
{
  QStringList result = m_profiles.keys();
  result.sort();
  return result;
}
