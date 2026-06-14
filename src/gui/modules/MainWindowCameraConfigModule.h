#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowCameraConfigModule : public MainWindowModuleBase
{
public:
  explicit MainWindowCameraConfigModule(MainWindowContext& context);

  void configureCameraSource(const CameraConfig& camera);
  void configureVimbaCamera(const CameraConfig& camera);
  void configureVimbaCameraSlot(int currentSlot, const QString& currentCameraId = {});
  void configureUsbCamera(const CameraConfig& camera);
  void configureUsbCameraSlot(int currentSlot, const QString& currentCameraId = {});
  void configureCameraSampleImage(const CameraConfig& camera);
  void configureCameraTestImages(const CameraConfig& camera);
  void acquireCameraSampleImage(const CameraConfig& camera);
};
