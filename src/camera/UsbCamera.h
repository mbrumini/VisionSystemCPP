#pragma once

#include "camera/ICamera.h"
#include "config/AppConfig.h"

#include <opencv2/videoio.hpp>

class UsbCamera : public ICamera
{
public:
  explicit UsbCamera(CameraConfig config);

  bool open() override;
  bool getFrame(cv::Mat& frame) override;
  void close() override;
  bool setAcquisitionSettings(const CameraAcquisitionConfig& acquisition);

private:
  void applyAcquisitionSettings();

  CameraConfig m_config;
  cv::VideoCapture m_capture;
};
