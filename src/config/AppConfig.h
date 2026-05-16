#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct ProcessingProfile
{
  QString id;
  QString imageMode;
  QStringList inspectionTypes;
  QStringList guiTools;
};

struct CameraConfig
{
  QString id;
  QString displayName;
  bool exists = false;
  bool enabled = false;
  QString type;
  QString folder;
  QString processingProfileId;
  ProcessingProfile profile;
};

class AppConfig
{
public:
  bool load(const QString& filePath, QString* errorMessage = nullptr);

  int maxCameras() const;
  const QVector<CameraConfig>& cameras() const;
  QVector<CameraConfig> activeCameras() const;

private:
  int m_maxCameras = 0;
  QVector<CameraConfig> m_cameras;
};
