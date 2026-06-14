#pragma once

#include "camera/CameraDeviceInfo.h"

#include <QString>
#include <QStringList>
#include <QVector>

class VimbaXDiscovery
{
public:
  static bool isSdkAvailable();
  static QVector<CameraDeviceInfo> discover(
    QString* errorMessage = nullptr,
    QStringList* diagnostics = nullptr);
};
