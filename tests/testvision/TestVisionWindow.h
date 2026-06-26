#pragma once

#include "TestVisionTypes.h"
#include "TestVisionExecutionProfile.h"
#include "TestVisionPlotWidgets.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <QByteArray>

#include <opencv2/core.hpp>

#include <functional>
#include <map>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class TestVisionWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit TestVisionWindow(const QString& scenarioPath, QWidget* parent = nullptr);
  ~TestVisionWindow() override;

  void setSendOnlyMode(bool enabled);
  bool sendOnlyMode() const;

  void startCampaignWorker(
    const QJsonObject& item,
    int cycle,
    std::function<void(const QJsonObject&)> completion,
    std::function<void(const QString&, const TestResult&)> resultCallback = {});

  bool loadCampaignFromFile(const QString& path, QString* error, bool autoStart = false);
  void startBroadcastFromCameras(const QStringList& cameras);

private:
  enum class State { Idle, WaitingHello, WaitingRecipe, WaitingAccepted, WaitingResult };

  QDoubleSpinBox* doubleSpin(double value, double minimum, double maximum, double step, int decimals = 3);
  void buildUi();
  TestVisionExecutionProfile currentExecutionProfile() const;
  void applyExecutionProfile(const TestVisionExecutionProfile& profile);
  void saveExecutionProfileToScenario();
  void saveExecutionProfileToFile();
  void loadExecutionProfileFromFile();
  bool loadScenario(QString* error);
  void setCanvasSize(int width, int height, int background = -1);
  void syncResolutionComboFromCanvas();
  void applySelectedResolutionPreset();
  void onCanvasSpinboxChanged();
  void updateCanvasSizeLabel();
  void updateMasterShape(bool persistAsset = false);
  void updateScenarioLabel();
  bool connectPipe();
  void sendSample();
  void writeSample();
  void updatePlanCount();
  bool buildPlan(QString* error);
  void generateAiDataset();
  void loadCampaign();
  void editCampaign();
  void applyCampaignItem(const QJsonObject& item);
  void selectBroadcastCamera(const QString& cameraId);
  QJsonArray selectedBroadcastItems() const;
  void startCampaign();
  void startParallelCampaign();
  void launchNextParallelBatch();
  void runNextCampaignItem();
  QJsonObject buildCampaignSummary(const QJsonObject& item, int cycle, const QString& pipelineFailure = {}) const;
  void recordCampaignSummary();
  void finishCampaign();
  QJsonObject currentBroadcastItem(const QString& cameraId, const QString& strategyId) const;
  void appendBroadcastResult(const QString& cameraId, const TestResult& result);
  void refreshBroadcastAnalysisSummary();
  void refreshBroadcastAnalysisSelection();
  void startBroadcastTest(const QJsonArray& targets);
  void syncStrategyFromRecipe();
  void startTest();
  void stopTest(const QString& reason);
  void closePipe();
  void pollPipe();
  void processMessage(const QJsonObject& message);
  void sendNextFrame();
  void finishSendOnlyTest();
  void requestRecipe();
  void handleResult(const QJsonObject& message);
  void scheduleDeferredAnalysisRefresh();
  void refreshMeasurementAnalysis(bool rebuildTable);
  void appendMeasurementRowsForResult(const TestResult& result);
  void appendResultRow(const TestResult& result);
  void refreshAnalysis(bool rebuildTables);
  void finishTest();
  void saveReport();
  void appendLog(const QString& message);

  QString m_scenarioPath;
  QJsonObject m_scenario;
  cv::Mat m_master;
  cv::Mat m_currentImage;
  QVector<TestPose> m_plan;
  QVector<TestResult> m_results;
  TestPose m_currentPose;
  int m_currentIndex = -1;
  int m_currentFrameId = 0;
  double m_expectedX = 0.0;
  double m_expectedY = 0.0;
  double m_expectedAngle = 0.0;
  bool m_hasBaseline = false;
  TestPose m_baselinePose;
  TestResult m_baselineResult;
  QHash<QString, double> m_baselineMeasurementPixels;
  QHash<QString, double> m_recipeMeasurementNominals;

  QLabel* m_preview = nullptr;
  QLabel* m_scenarioLabel = nullptr;
  QLabel* m_frameLabel = nullptr;
  QLabel* m_expectedLabel = nullptr;
  QLabel* m_actualLabel = nullptr;
  QLabel* m_stateLabel = nullptr;
  QLabel* m_totalLabel = nullptr;
  QLabel* m_summaryLabel = nullptr;
  QPlainTextEdit* m_log = nullptr;
  QPushButton* m_startButton = nullptr;
  QPushButton* m_stopButton = nullptr;
  QTableWidget* m_table = nullptr;
  QTableWidget* m_measurementTable = nullptr;
  QLabel* m_measurementSummaryLabel = nullptr;
  MeasurementStabilityPlotWidget* m_measurementPlot = nullptr;
  PlotWidget* m_anglePlot = nullptr;
  PlotWidget* m_centerPlot = nullptr;
  RepeatabilityPlotWidget* m_repeatabilityPlot = nullptr;
  QDoubleSpinBox* m_xMin = nullptr;
  QDoubleSpinBox* m_xMax = nullptr;
  QDoubleSpinBox* m_xStep = nullptr;
  QDoubleSpinBox* m_yMin = nullptr;
  QDoubleSpinBox* m_yMax = nullptr;
  QDoubleSpinBox* m_yStep = nullptr;
  QDoubleSpinBox* m_angleMin = nullptr;
  QDoubleSpinBox* m_angleMax = nullptr;
  QDoubleSpinBox* m_angleStep = nullptr;
  QDoubleSpinBox* m_pixelSize = nullptr;
  QSpinBox* m_passes = nullptr;
  QSpinBox* m_intervalMs = nullptr;
  QComboBox* m_resolutionCombo = nullptr;
  QSpinBox* m_canvasWidthSpin = nullptr;
  QSpinBox* m_canvasHeightSpin = nullptr;
  QLabel* m_canvasSizeLabel = nullptr;
  QComboBox* m_shapeCombo = nullptr;
  QComboBox* m_strategyCombo = nullptr;
  QComboBox* m_recipeCombo = nullptr;
  QComboBox* m_cameraCombo = nullptr;
  QTableWidget* m_cameraTargets = nullptr;
  QTableWidget* m_broadcastTable = nullptr;
  QTableWidget* m_multiSummaryTable = nullptr;
  QComboBox* m_multiPrimaryCamera = nullptr;
  QComboBox* m_multiCompareCamera = nullptr;
  QLabel* m_multiPrimarySummary = nullptr;
  QLabel* m_multiCompareSummary = nullptr;
  PlotWidget* m_multiPrimaryAnglePlot = nullptr;
  PlotWidget* m_multiPrimaryCenterPlot = nullptr;
  PlotWidget* m_multiCompareAnglePlot = nullptr;
  PlotWidget* m_multiCompareCenterPlot = nullptr;
  QGroupBox* m_multiCompareGroup = nullptr;
  QPushButton* m_sendSampleButton = nullptr;
  QPushButton* m_generateDatasetButton = nullptr;
  QPushButton* m_loadCampaignButton = nullptr;
  QPushButton* m_editCampaignButton = nullptr;
  QPushButton* m_startCampaignButton = nullptr;
  QCheckBox* m_sendOnlyCheck = nullptr;
  int m_canvasWidth = 640;
  int m_canvasHeight = 480;
  int m_canvasBackground = 240;
  bool m_syncingResolution = false;
  bool m_pendingSample = false;
  bool m_sendOnlyMode = false;
  QString m_expectedRecipeId;
  QString m_lastProcessingError;
  QString m_campaignPath;
  QJsonObject m_campaign;
  QJsonArray m_campaignItems;
  QJsonArray m_campaignSummaries;
  int m_campaignCycles = 1;
  int m_campaignCycle = 0;
  int m_campaignItemIndex = -1;
  bool m_campaignRunning = false;
  bool m_abortCurrentCampaignItem = false;
  QString m_campaignItemFailure;
  QJsonArray m_parallelPendingItems;
  int m_activeCampaignWorkers = 0;
  bool m_isCampaignWorker = false;
  bool m_workerCompleted = false;
  int m_workerCycle = 0;
  QJsonObject m_workerItem;
  QString m_workerFailure;
  std::function<void(const QJsonObject&)> m_workerCompletion;
  std::function<void(const QString&, const TestResult&)> m_workerResultCallback;
  bool m_broadcastRunning = false;
  int m_activeBroadcastWorkers = 0;
  QJsonArray m_broadcastSummaries;
  QMap<QString, QVector<TestResult>> m_broadcastResultsByCamera;
  QList<TestVisionWindow*> m_broadcastWorkers;
  QPixmap m_previewPixmap;
  QTimer m_pollTimer;
  QTimer m_sendTimer;
  QTimer m_analysisRefreshTimer;
  QHash<int, SentFrameInfo> m_sentFrames;
  void* m_pipe = nullptr;
  QByteArray m_buffer;
  State m_state = State::Idle;
};
