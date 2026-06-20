#pragma once

#include "ai/AiMaskLabelStorage.h"
#include "gui/modules/MainWindowModuleBase.h"
#include "processing/MaskPoseEstimator.h"
#include "runtime/CameraRuntime.h"

#include <QPointer>
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

class CameraConfig;
class QLabel;
class QProcess;
class QPushButton;
class QTimer;
class SetupResultsDialog;

struct AiLocalizationFrameResult
{
  bool processed = false;
  bool found = false;
  QString error;
  QString imagePath;
  QString maskPath;
  QString referenceMaskPath;
  double confidence = 0.0;
  double referenceConfidence = 0.0;
  double elapsedMs = 0.0;
  MaskPoseResult pose;
  bool hasOrientationReference = false;
  cv::Point2d orientationReferenceCenter;
  cv::Mat diagnosticImage;
};

class MainWindowSetupModule : public MainWindowModuleBase
{
public:
  explicit MainWindowSetupModule(MainWindowContext& context);

  static QString runtimeStatusText(const MainWindowContext& context, CameraRuntime::Status status);

  void showCameraSetupPanel(const CameraConfig& camera);
  void showToolPanel(const CameraConfig& camera, const QString& toolId);
  void showAiPanel(const CameraConfig& camera);
  void showAiLocalizationPanel(const CameraConfig& camera);
  void showAiLocalizationTrainingPanel(const CameraConfig& camera);
  void advanceAiLocalizationLabeling();
  void handleAiLocalizationPolygon(const QVector<QPoint>& polygon);
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
  void runAiLocalizationInferenceFrame(
    const CameraConfig& camera,
    const cv::Mat& frame,
    std::function<void(const AiLocalizationFrameResult&)> callback);

private:
  void startAiLocalizationLabeling(const CameraConfig& camera);
  void loadAiLocalizationLabelingImage(int index);
  void updateAiLocalizationLabelingStatus();
  void beginAiLocalizationPolygon(int classId);
  void finishAiLocalizationLabelingImage();
  void updateAiLocalizationPolygonOverlay();
  void startAiLocalizationTraining(
    const CameraConfig& camera,
    int epochs,
    int imageSize,
    int batchSize,
    const QString& device,
    double valRatio,
    const QString& baseModel);
  void runAiLocalizationInference(const CameraConfig& camera);
  bool ensureAiLocalizationInferenceWorker(const QString& modelPath);
  void stopAiLocalizationInferenceWorker();
  void handleAiLocalizationInferenceOutput();
  void applyAiLocalizationInferenceResult(
    const CameraConfig& camera,
    const AiLocalizationFrameResult& result);
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
  void updateAiLocalizationTrainingGraph(const QString& cameraId);
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
  QProcess* m_aiLocalizationInferenceProcess = nullptr;
  QPointer<QLabel> m_aiInferenceResultLabel;
  QPointer<QLabel> m_aiLocalizationInferenceResultLabel;
  QString m_aiInferenceModelPath;
  QString m_aiLocalizationInferenceModelPath;
  QString m_aiLocalizationInferenceImagePath;
  CameraConfig m_aiLocalizationInferenceCamera;
  QHash<QString, std::function<void(const AiLocalizationFrameResult&)>>
    m_aiLocalizationInferenceCallbacks;
  QString m_aiTrainingCameraId;
  QString m_aiLocalizationTrainingCameraId;
  QString m_aiTrainingPreviousModelPath;
  QString m_aiTrainingGraphCameraId;
  bool m_aiTrainingGraphIsLocalization = false;
  QString m_aiClassificationCaptureCameraId;
  CameraConfig m_aiLocalizationLabelingCamera;
  QStringList m_aiLocalizationLabelingImages;
  int m_aiLocalizationLabelingIndex = -1;
  QPointer<QLabel> m_aiLocalizationLabelingStatus;
  QPointer<QPushButton> m_aiLocalizationPreviousButton;
  QPointer<QPushButton> m_aiLocalizationNextButton;
  QPointer<QPushButton> m_aiLocalizationPieceButton;
  QPointer<QPushButton> m_aiLocalizationReferenceButton;
  QPointer<QPushButton> m_aiLocalizationFinishButton;
  QVector<AiSegmentationPolygon> m_aiLocalizationPolygons;
  int m_aiLocalizationActiveClassId = 0;
  CameraConfig m_aiClassificationCaptureCamera;
  bool m_aiClassificationCaptureToClass = false;
  AiClassificationClassConfig m_aiClassificationCaptureClass;
};
