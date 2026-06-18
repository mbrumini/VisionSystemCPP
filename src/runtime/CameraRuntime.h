#pragma once

#include "camera/ICamera.h"
#include "config/AppConfig.h"
#include "geometry/GeometrySet.h"
#include "runtime/PartPose.h"

#include <QString>

#include <memory>

class CameraRuntime
{
public:
  enum class Status
  {
    Ready,
    Running,
    Stopped,
    Error
  };

  bool start(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage = nullptr);
  void stop();
  bool step(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage = nullptr);
  bool applyAcquisitionSettings(
    const CameraAcquisitionConfig& acquisition,
    QString* errorMessage = nullptr);

  bool running() const;
  bool loop() const;
  void setLoop(bool loop);

  int intervalMs() const;
  void setIntervalMs(int intervalMs);

  int frameIndex() const;
  Status status() const;
  const cv::Mat& currentFrame() const;
  void clearCurrentFrame();
  const PartPose& currentPose() const;
  void setCurrentPose(const PartPose& pose);
  void clearCurrentPose(const QString& cameraId = {});
  const GeometrySet& geometries() const;
  GeometrySet& geometries();
  void clearGeometries();

private:
  bool ensureSource(const CameraConfig& camera, const QString& resolvedFolder, QString* errorMessage);

  bool m_running = false;
  bool m_loop = true;
  int m_intervalMs = 500;
  int m_frameIndex = 0;
  Status m_status = Status::Stopped;
  cv::Mat m_currentFrame;
  PartPose m_currentPose;
  GeometrySet m_geometries;
  std::unique_ptr<ICamera> m_source;
  QString m_sourceType;
  QString m_sourceFolder;
  QString m_sourceDeviceId;
  int m_sourceUsbIndex = -1;
};
