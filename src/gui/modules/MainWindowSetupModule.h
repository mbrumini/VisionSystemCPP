#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "runtime/CameraRuntime.h"

#include <QPointer>
#include <QString>

class CameraConfig;
class QProcess;
class QTimer;
class SetupResultsDialog;

class MainWindowSetupModule : public MainWindowModuleBase
{
public:
  explicit MainWindowSetupModule(MainWindowContext& context);

  static QString runtimeStatusText(const MainWindowContext& context, CameraRuntime::Status status);

  void showCameraSetupPanel(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void showAiPanel(const CameraConfig& camera);
  void showAiClassificationPanel(const CameraConfig& camera);
  void showAiPlaceholderPanel(const CameraConfig& camera, const QString& toolId);
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
  void startAiClassificationCapture(const CameraConfig& camera, bool toClass, const AiClassificationClassConfig& classConfig = {});
  void stopAiClassificationCapture();
  void captureAiClassificationFrame();
  void prepareAiClassificationDataset(const CameraConfig& camera);
  void startAiClassificationTraining(const CameraConfig& camera);
  void runAiClassificationInference(const CameraConfig& camera);
  void startAiProcess(const QString& label, const QString& program, const QStringList& arguments);

  QPointer<SetupResultsDialog> m_resultsDialog;
  QTimer* m_aiClassificationCaptureTimer = nullptr;
  QProcess* m_aiProcess = nullptr;
  QString m_aiClassificationCaptureCameraId;
  bool m_aiClassificationCaptureToClass = false;
  AiClassificationClassConfig m_aiClassificationCaptureClass;
};
