#pragma once

#include "camera/ICamera.h"
#include "config/AppConfig.h"
#include "geometry/GeometrySet.h"
#include "runtime/PartPose.h"
#include "simulator/SimulatorProtocol.h"

#include <QString>

#include <memory>
#include <mutex>

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
  bool grabFrame(const CameraConfig& camera, const QString& resolvedFolder, cv::Mat& frame, SimulatorFrameMetadata& metadata, QString& error);
  void updateStateAfterGrab(const cv::Mat& frame, const SimulatorFrameMetadata& metadata);
  bool applyAcquisitionSettings(
    const CameraAcquisitionConfig& acquisition,
    QString* errorMessage = nullptr);

  bool running() const;
  std::shared_ptr<ICamera> source() const;
  bool loop() const;
  void setLoop(bool loop);

  int intervalMs() const;
  void setIntervalMs(int intervalMs);

  int frameIndex() const;
  Status status() const;
  const cv::Mat& currentFrame() const;
  void setCurrentFrame(const cv::Mat& frame);
  const SimulatorFrameMetadata& currentSimulatorFrame() const;
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
  SimulatorFrameMetadata m_currentSimulatorFrame;
  PartPose m_currentPose;
  GeometrySet m_geometries;
  std::shared_ptr<ICamera> m_source;
  QString m_sourceType;
  QString m_sourceFolder;
  QString m_sourceDeviceId;
  int m_sourceUsbIndex = -1;
  mutable std::mutex m_sourceMutex;
};
