#pragma once

#include "config/AppConfig.h"

#include <QString>

#include <memory>

class ICamera;

class CameraFactory
{
public:
  static QString sourceKind(const CameraConfig& camera);
  static QString sourceIdentity(const CameraConfig& camera, const QString& resolvedFolder);
  static std::shared_ptr<ICamera> create(
    const CameraConfig& camera,
    const QString& resolvedFolder,
    bool loop,
    QString* errorMessage = nullptr);
};
