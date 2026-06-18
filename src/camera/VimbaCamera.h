#pragma once

#include "camera/ICamera.h"
#include "config/AppConfig.h"

#include <QString>

#ifdef VISION_WITH_VIMBAX
#include <VmbCPP/VmbCPP.h>
#include <condition_variable>
#include <mutex>
#endif

class VimbaCamera : public ICamera
{
public:
  explicit VimbaCamera(CameraConfig config);
  ~VimbaCamera() override;

  bool open() override;
  bool getFrame(cv::Mat& frame) override;
  void close() override;
  QString lastError() const;

private:
#ifdef VISION_WITH_VIMBAX
  class FrameObserver;

  bool copyFrameToMat(const VmbCPP::FramePtr& vimbaFrame, cv::Mat& frame);
  bool setPreferredPixelFormat();
  void applyAcquisitionSettings();
  bool prepareCapture();

  CameraConfig m_config;
  VmbCPP::CameraPtr m_camera;
  VmbCPP::FramePtr m_frame;
  VmbCPP::IFrameObserverPtr m_observer;
  bool m_systemStarted = false;
  bool m_captureStarted = false;
  bool m_acquisitionStarted = false;
#else
  CameraConfig m_config;
#endif
  QString m_lastError;
};
