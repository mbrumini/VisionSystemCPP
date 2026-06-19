#pragma once

#include "camera/ICamera.h"
#include "simulator/SimulatorProtocol.h"

#include <QString>

class SimulatedCamera : public ICamera
{
public:
  explicit SimulatedCamera(QString channel);

  bool open() override;
  bool getFrame(cv::Mat& frame) override;
  void close() override;

  const SimulatorFrameMetadata& lastFrameMetadata() const;

private:
  QString m_channel;
  bool m_open = false;
  SimulatorFrameMetadata m_lastFrameMetadata;
};
