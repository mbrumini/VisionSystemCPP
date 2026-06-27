#include "camera/UsbCameraDiscovery.h"

#include <QtGlobal>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
void addDiagnostic(QStringList* diagnostics, const QString& message)
{
  if (diagnostics)
  {
    diagnostics->append(message);
  }
}

const std::vector<std::pair<int, int>>& candidateResolutions()
{
  static const std::vector<std::pair<int, int>> resolutions = {
    {4096, 3000},
    {3840, 2160},
    {2592, 1944},
    {2048, 1536},
    {1920, 1080},
    {1600, 1200},
    {1280, 1024},
    {1280, 720},
    {1024, 768},
    {800, 600},
    {640, 480},
  };
  return resolutions;
}

bool grabFrame(cv::VideoCapture& capture, cv::Mat& frame)
{
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    capture >> frame;
    if (!frame.empty())
    {
      return true;
    }
  }
  return false;
}

qint64 pixelCount(int width, int height)
{
  return static_cast<qint64>(width) * static_cast<qint64>(height);
}
}

UsbCameraResolution UsbCameraDiscovery::probeBestResolution(cv::VideoCapture& capture)
{
  UsbCameraResolution best;
  if (!capture.isOpened())
  {
    return best;
  }

  cv::Mat frame;
  if (grabFrame(capture, frame))
  {
    best.width = frame.cols;
    best.height = frame.rows;
  }

  for (const std::pair<int, int>& candidate : candidateResolutions())
  {
    capture.set(cv::CAP_PROP_FRAME_WIDTH, candidate.first);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, candidate.second);
    if (!grabFrame(capture, frame))
    {
      continue;
    }

    const qint64 candidatePixels = pixelCount(frame.cols, frame.rows);
    const qint64 bestPixels = pixelCount(best.width, best.height);
    if (candidatePixels > bestPixels)
    {
      best.width = frame.cols;
      best.height = frame.rows;
    }
  }

  if (best.width > 0 && best.height > 0)
  {
    capture.set(cv::CAP_PROP_FRAME_WIDTH, best.width);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, best.height);
    grabFrame(capture, frame);
    if (!frame.empty())
    {
      best.width = frame.cols;
      best.height = frame.rows;
    }
  }

  return best;
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

    const int defaultWidth = frame.cols;
    const int defaultHeight = frame.rows;
    const UsbCameraResolution resolution = probeBestResolution(capture);

    UsbCameraDeviceInfo info;
    info.index = index;
    info.displayName = QString("USB Camera %1").arg(index);
    info.width = resolution.width > 0 ? resolution.width : defaultWidth;
    info.height = resolution.height > 0 ? resolution.height : defaultHeight;
    info.fps = capture.get(cv::CAP_PROP_FPS);
    devices.append(info);
    addDiagnostic(
      diagnostics,
      QString("USB camera found: index=%1 default=%2x%3 negotiated=%4x%5 fps=%6")
        .arg(info.index)
        .arg(defaultWidth)
        .arg(defaultHeight)
        .arg(info.width)
        .arg(info.height)
        .arg(info.fps, 0, 'f', 2));

    capture.release();
  }

  addDiagnostic(diagnostics, QString("USB camera discovery end: count=%1").arg(devices.size()));
  return devices;
}
