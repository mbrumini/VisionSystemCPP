#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct CameraTriggerConfig
{
  QString mode;
  QString source;
  QString ioOutput;
  QString cameraLine;
};

struct ProcessingProfile
{
  QString id;
  QString imageMode;
  QStringList inspectionTypes;
  QStringList guiTools;
};

struct CameraConfig
{
  int slot = 0;
  QString id;
  QString displayName;
  bool exists = false;
  bool enabled = false;
  QString type;
  QString folder;
  int usbIndex = -1;
  QString serial;
  QString deviceId;
  QString modelName;
  QString interfaceId;
  CameraTriggerConfig trigger;
  QString processingProfileId;
  ProcessingProfile profile;
};

class AppConfig
{
public:
  bool load(const QString& filePath, QString* errorMessage = nullptr);
  bool saveCameraSource(
    const QString& filePath,
    const QString& cameraId,
    const QString& type,
    const QString& folder,
    QString* errorMessage = nullptr);
  bool saveVimbaCameraAssignment(
    const QString& filePath,
    int slot,
    const QString& deviceId,
    const QString& serial,
    const QString& displayName,
    const QString& modelName,
    const QString& interfaceId,
    bool enabled,
    QString* errorMessage = nullptr);
  bool saveUsbCameraAssignment(
    const QString& filePath,
    int slot,
    int usbIndex,
    const QString& displayName,
    bool enabled,
    QString* errorMessage = nullptr);
  bool saveCameraSystemSettings(
    const QString& filePath,
    const QVector<CameraConfig>& cameras,
    QString* errorMessage = nullptr);

  int maxCameras() const;
  const QVector<CameraConfig>& cameras() const;
  QVector<CameraConfig> activeCameras() const;

private:
  int m_maxCameras = 0;
  QVector<CameraConfig> m_cameras;
};
