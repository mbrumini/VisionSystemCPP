#include "camera/UsbCameraDiscovery.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace
{
void addDiagnostic(QStringList* diagnostics, const QString& message)
{
  if (diagnostics)
  {
    diagnostics->append(message);
  }
}
}

QVector<UsbCameraDeviceInfo> UsbCameraDiscovery::discover(int maxIndex, QStringList* diagnostics)
{
  QVector<UsbCameraDeviceInfo> devices;
  addDiagnostic(diagnostics, QString("USB camera discovery begin: maxIndex=%1").arg(maxIndex));

  for (int index = 0; index < maxIndex; ++index)
  {
    addDiagnostic(diagnostics, QString("USB camera probe begin: index=%1").arg(index));

    cv::VideoCapture capture;
#ifdef _WIN32
    bool opened = capture.open(index, cv::CAP_DSHOW);
    if (!opened)
    {
      opened = capture.open(index);
    }
#else
    const bool opened = capture.open(index);
#endif

    if (!opened)
    {
      addDiagnostic(diagnostics, QString("USB camera probe closed: index=%1").arg(index));
      continue;
    }

    cv::Mat frame;
    capture >> frame;
    if (frame.empty())
    {
      addDiagnostic(diagnostics, QString("USB camera probe no frame: index=%1").arg(index));
      capture.release();
      continue;
    }

    UsbCameraDeviceInfo info;
    info.index = index;
    info.displayName = QString("USB Camera %1").arg(index);
    info.width = frame.cols;
    info.height = frame.rows;
    info.fps = capture.get(cv::CAP_PROP_FPS);
    devices.append(info);
    addDiagnostic(
      diagnostics,
      QString("USB camera found: index=%1 size=%2x%3 fps=%4")
        .arg(info.index)
        .arg(info.width)
        .arg(info.height)
        .arg(info.fps, 0, 'f', 2));

    capture.release();
  }

  addDiagnostic(diagnostics, QString("USB camera discovery end: count=%1").arg(devices.size()));
  return devices;
}
