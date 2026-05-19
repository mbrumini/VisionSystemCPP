#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowCameraConfigModule : public MainWindowModuleBase
{
public:
  explicit MainWindowCameraConfigModule(MainWindowContext& context);

  void configureCameraSource(const CameraConfig& camera);
  void configureCameraSampleImage(const CameraConfig& camera);
  void configureCameraTestImages(const CameraConfig& camera);
  void acquireCameraSampleImage(const CameraConfig& camera);
};
