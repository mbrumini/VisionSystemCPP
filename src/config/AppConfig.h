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

struct CameraCalibrationConfig
{
  bool enabled = false;
  QString type = "none";
  QString file;
  double pixelSizeXMm = 0.0;
  double pixelSizeYMm = 0.0;
  QString updatedAt;
};

struct CameraAcquisitionConfig
{
  bool autoExposure = true;
  bool hasExposure = false;
  double exposure = 0.0;
  bool autoGain = true;
  bool hasGain = false;
  double gain = 0.0;
  bool autoWhiteBalance = true;
  bool hasWhiteBalance = false;
  double whiteBalance = 0.0;
  int frameWidth = 0;
  int frameHeight = 0;
  int frameIntervalMs = 500;
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
  QString backend;
  QString cameraProfileId;
  QString folder;
  int usbIndex = -1;
  QString serial;
  QString deviceId;
  QString modelName;
  QString interfaceId;
  QString simulatorChannel;
  CameraTriggerConfig trigger;
  CameraCalibrationConfig calibration;
  CameraAcquisitionConfig acquisition;
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
    int frameWidth,
    int frameHeight,
    bool enabled,
    QString* errorMessage = nullptr);
  bool saveCameraAcquisitionSettings(
    const QString& filePath,
    const QString& cameraId,
    const CameraAcquisitionConfig& acquisition,
    QString* errorMessage = nullptr);
  bool updateCameraAcquisitionSettings(
    const QString& cameraId,
    const CameraAcquisitionConfig& acquisition);
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
