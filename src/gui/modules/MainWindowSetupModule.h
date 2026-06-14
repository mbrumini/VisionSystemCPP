#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "runtime/CameraRuntime.h"

#include <QPointer>
#include <QString>

class CameraConfig;
class SetupResultsDialog;

class MainWindowSetupModule : public MainWindowModuleBase
{
public:
  explicit MainWindowSetupModule(MainWindowContext& context);

  static QString runtimeStatusText(const MainWindowContext& context, CameraRuntime::Status status);

  void showCameraSetupPanel(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void startCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel = true);
  void stopCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel = true);
  void showCameraSampleImage(const CameraConfig& camera);
  void stepCameraSimulation(const CameraConfig& camera);
  void advanceCameraFrame(const CameraConfig& camera);
  void processCurrentCameraFrame(const CameraConfig& camera);
  void refreshSetupGeometryResults(const CameraConfig& camera);
  void refreshPoseForCurrentFrame(const CameraConfig& camera);
  void updateCameraSetupDetails(const CameraConfig& camera);
  QString cameraSetupDetailsText(const CameraConfig& camera) const;
  void showSetupResultsPopup(const CameraConfig& camera);
  void updateSetupResultsPopup(const CameraConfig& camera);

private:
  QPointer<SetupResultsDialog> m_resultsDialog;
};
