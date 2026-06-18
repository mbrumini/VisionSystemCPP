#pragma once

#include "gui/modules/MainWindowModuleBase.h"
#include "runtime/CameraRuntime.h"

#include <QPointer>
#include <QString>

class CameraConfig;
class QLabel;
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
  void showAiClassificationTrainingPanel(const CameraConfig& camera);
  void showAiSegmentationPanel(const CameraConfig& camera);
  void showAiPlaceholderPanel(const CameraConfig& camera, const QString& toolId);
  void showCameraAcquisitionPanel(const CameraConfig& camera);
  void stopAiClassificationCapture();
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
  void captureAiClassificationFrame();
  void prepareAiClassificationDataset(const CameraConfig& camera);
  void startAiClassificationTraining(
    const CameraConfig& camera,
    int epochs = 100,
    int imageSize = 224,
    int batchSize = 8,
    const QString& device = "0",
    double valRatio = 0.2,
    const QString& baseModel = "yolo11s-cls.pt");
  void runAiClassificationInference(const CameraConfig& camera);
  void startAiProcess(const QString& label, const QString& program, const QStringList& arguments);
  void chooseAiClassificationModel(const CameraConfig& camera);
  void handleAiTrainingFinished(int code);
  void updateAiTrainingGraph(const QString& cameraId);
  void stopAiTrainingGraphUpdates();
  bool ensureAiInferenceWorker(const QString& modelPath);
  void stopAiInferenceWorker();
  void handleAiInferenceOutput();
  void sendAiInferenceRequest(const QString& imagePath);

  QPointer<SetupResultsDialog> m_resultsDialog;
  QTimer* m_aiClassificationCaptureTimer = nullptr;
  QTimer* m_aiTrainingGraphTimer = nullptr;
  QProcess* m_aiProcess = nullptr;
  QProcess* m_aiInferenceProcess = nullptr;
  QPointer<QLabel> m_aiInferenceResultLabel;
  QString m_aiInferenceModelPath;
  QString m_aiTrainingCameraId;
  QString m_aiTrainingPreviousModelPath;
  QString m_aiTrainingGraphCameraId;
  QString m_aiClassificationCaptureCameraId;
  CameraConfig m_aiClassificationCaptureCamera;
  bool m_aiClassificationCaptureToClass = false;
  AiClassificationClassConfig m_aiClassificationCaptureClass;
};
