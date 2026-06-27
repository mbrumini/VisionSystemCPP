#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace cv
{
class VideoCapture;
}

struct UsbCameraDeviceInfo
{
  int index = -1;
  QString displayName;
  int width = 0;
  int height = 0;
  double fps = 0.0;
};

struct UsbCameraResolution
{
  int width = 0;
  int height = 0;
};

class UsbCameraDiscovery
{
public:
  static QVector<UsbCameraDeviceInfo> discover(
    int maxIndex = 10,
    QStringList* diagnostics = nullptr);

  // Negotiates the highest resolution the driver accepts (DirectShow often defaults to 640x480).
  static UsbCameraResolution probeBestResolution(cv::VideoCapture& capture);
};
