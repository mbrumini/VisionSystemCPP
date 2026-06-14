#pragma once

#include "camera/ICamera.h"

#include <opencv2/videoio.hpp>

class UsbCamera : public ICamera
{
public:
  explicit UsbCamera(int index);

  bool open() override;
  bool getFrame(cv::Mat& frame) override;
  void close() override;

private:
  int m_index = 0;
  cv::VideoCapture m_capture;
};
