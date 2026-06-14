#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct UsbCameraDeviceInfo
{
  int index = -1;
  QString displayName;
  int width = 0;
  int height = 0;
  double fps = 0.0;
};

class UsbCameraDiscovery
{
public:
  static QVector<UsbCameraDeviceInfo> discover(
    int maxIndex = 10,
    QStringList* diagnostics = nullptr);
};
