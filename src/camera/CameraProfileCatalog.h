#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

struct CameraProfileLimits
{
  bool hasExposureUsMin = false;
  bool hasExposureUsMax = false;
  bool hasGainMin = false;
  bool hasGainMax = false;
  double exposureUsMin = 0.0;
  double exposureUsMax = 0.0;
  double gainMin = 0.0;
  double gainMax = 0.0;
};

struct CameraProfileFeatures
{
  bool softwareTrigger = false;
  bool hardwareTrigger = false;
  bool continuousAcquisition = true;
  bool packetSize = false;
  bool packetDelay = false;
};

struct CameraDriverProfile
{
  QString id;
  QString backend;
  QString vendor;
  QString family;
  QString transport;
  QString modelContains;
  QString vendorContains;
  QString interfaceContains;
  CameraProfileLimits limits;
  CameraProfileFeatures features;
  QHash<QString, QString> commands;
};

class CameraProfileCatalog
{
public:
  bool load(const QString& filePath, QString* errorMessage = nullptr);
  bool isEmpty() const;
  const CameraDriverProfile* profile(const QString& id) const;
  QStringList ids() const;

private:
  QHash<QString, CameraDriverProfile> m_profiles;
};
