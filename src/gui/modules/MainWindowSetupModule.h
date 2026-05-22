#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "runtime/CameraRuntime.h"

#include <QString>

class CameraConfig;

class MainWindowSetupModule : public MainWindowModuleBase
{
public:
  explicit MainWindowSetupModule(MainWindowContext& context);

  static QString runtimeStatusText(const MainWindowContext& context, CameraRuntime::Status status);

  void showCameraSetupPanel(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void startCameraSimulation(const CameraConfig& camera);
  void stopCameraSimulation(const CameraConfig& camera);
  void stepCameraSimulation(const CameraConfig& camera);
  void advanceCameraFrame(const CameraConfig& camera);
  void processCurrentCameraFrame(const CameraConfig& camera);
  void refreshSetupGeometryResults(const CameraConfig& camera);
  void refreshPoseForCurrentFrame(const CameraConfig& camera);
  void updateCameraSetupDetails(const CameraConfig& camera);
  QString cameraSetupDetailsText(const CameraConfig& camera) const;
};
