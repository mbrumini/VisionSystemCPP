#include "camera/UsbCamera.h"

#include "camera/UsbCameraDiscovery.h"

UsbCamera::UsbCamera(CameraConfig config)
  : m_config(std::move(config))
{
}

bool UsbCamera::open()
{
#ifdef _WIN32
  if (!m_capture.open(m_config.usbIndex, cv::CAP_DSHOW))
  {
    if (!m_capture.open(m_config.usbIndex))
    {
      return false;
    }
  }

  applyAcquisitionSettings();
  // The automatic path deliberately asks the driver for the largest accepted
  // format; manual width/height keeps setup repeatable for metrology tests.
  if (m_config.acquisition.frameWidth > 0 && m_config.acquisition.frameHeight > 0)
  {
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_config.acquisition.frameWidth);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_config.acquisition.frameHeight);
  }
  else
  {
    UsbCameraDiscovery::probeBestResolution(m_capture);
  }
  return true;
#else
  if (!m_capture.open(m_config.usbIndex))
  {
    return false;
  }
  applyAcquisitionSettings();
  if (m_config.acquisition.frameWidth > 0 && m_config.acquisition.frameHeight > 0)
  {
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_config.acquisition.frameWidth);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_config.acquisition.frameHeight);
  }
  else
  {
    UsbCameraDiscovery::probeBestResolution(m_capture);
  }
  return true;
#endif
}

bool UsbCamera::getFrame(cv::Mat& frame)
{
  if (!m_capture.isOpened())
  {
    return false;
  }

  m_capture >> frame;
  return !frame.empty();
}

void UsbCamera::close()
{
  if (m_capture.isOpened())
  {
    m_capture.release();
  }
}

bool UsbCamera::setAcquisitionSettings(const CameraAcquisitionConfig& acquisition)
{
  if (!m_capture.isOpened())
  {
    return false;
  }

  m_config.acquisition = acquisition;
  applyAcquisitionSettings();
  // Some USB drivers apply resolution changes only after stream restart, but
  // setting it here gives drivers that support live reconfiguration a chance.
  if (m_config.acquisition.frameWidth > 0 && m_config.acquisition.frameHeight > 0)
  {
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, m_config.acquisition.frameWidth);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, m_config.acquisition.frameHeight);
  }
  else
  {
    UsbCameraDiscovery::probeBestResolution(m_capture);
  }
  return true;
}

void UsbCamera::applyAcquisitionSettings()
{
  if (!m_capture.isOpened())
  {
    return;
  }

  const CameraAcquisitionConfig& acquisition = m_config.acquisition;

  // DirectShow commonly uses 0.25 for manual exposure and 0.75 for auto.
  m_capture.set(cv::CAP_PROP_AUTO_EXPOSURE, acquisition.autoExposure ? 0.75 : 0.25);
  if (!acquisition.autoExposure && acquisition.hasExposure)
  {
    m_capture.set(cv::CAP_PROP_EXPOSURE, acquisition.exposure);
  }

  if (!acquisition.autoGain && acquisition.hasGain)
  {
    m_capture.set(cv::CAP_PROP_GAIN, acquisition.gain);
  }

  m_capture.set(cv::CAP_PROP_AUTO_WB, acquisition.autoWhiteBalance ? 1.0 : 0.0);
  if (!acquisition.autoWhiteBalance && acquisition.hasWhiteBalance)
  {
    m_capture.set(cv::CAP_PROP_WB_TEMPERATURE, acquisition.whiteBalance);
  }
}
