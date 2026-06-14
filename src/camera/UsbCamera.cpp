#include "camera/UsbCamera.h"

UsbCamera::UsbCamera(int index)
  : m_index(index)
{
}

bool UsbCamera::open()
{
#ifdef _WIN32
  if (!m_capture.open(m_index, cv::CAP_DSHOW))
  {
    return m_capture.open(m_index);
  }

  return true;
#else
  return m_capture.open(m_index);
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
