#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "config/TranslationManager.h"
#include "access/AccessSession.h"
#include "gui/CameraSetupPanelWidget.h"
#include "gui/CameraStripWidget.h"
#include "gui/CommandToolbarWidget.h"
#include "gui/CameraTileWidget.h"
#include "gui/ImageViewWidget.h"
#include "gui/MeasurementResultsWidget.h"
#include "gui/ToolIconBarWidget.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowLocalizationModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowRecipeModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "processing/LocalizationProcessor.h"
#include "processing/SurfaceDefectProcessor.h"
#include "runtime/CameraRuntime.h"
#include "util/DetailedLogger.h"

#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <map>

class MainWindow : public QMainWindow
{
public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  void bindModules();
  void buildUi();
  void buildMenu();
  void rebuildUi();
  void changeLanguage(const QString& languageCode);
  void toggleFullScreen();
  void loadConfiguration();
  void showGridView();
  void selectCamera(const CameraConfig& camera);
  void startMachine();
  void stopMachine();
  void showAccessLogin();
  void showHelp();
  void updateControlPanel(const CameraConfig* camera);
  void updateMeasurementResults();
  void updateCameraStripStatus(const QString& cameraId);
  void deactivateImageDrawingTools();
  void showCameraToolList(const CameraConfig& camera);
  void showLocalizationStrategyList(const CameraConfig& camera);
  void clearToolPanel();
  void appendLog(const QString& message);
  void setDetailedLogEnabled(bool enabled);
  void setSetupDetailsVisible(bool visible);
  void updateLargePreview();
  void setThreadLimitPrompt();
  void incPendingJobs(const QString& cameraId);
  void decPendingJobs(const QString& cameraId);
  QString trText(const QString& key) const;

  AppConfig m_config;
  RecipeManager m_recipeManager;
  TranslationManager m_translations;
  DetailedLogger m_detailedLogger;
  AccessSession m_accessSession;

  MainWindowContext m_ctx;
  MainWindowImagingModule m_imaging;
  MainWindowRecipeModule m_recipes;
  MainWindowGeometryModule m_geometry;
  MainWindowSurfaceModule m_surface;
  MainWindowLocalizationModule m_localization;
  MainWindowCameraConfigModule m_cameraConfig;
  MainWindowConstructedGeometryModule m_constructedGeometry;
  MainWindowMeasurementModule m_measurement;
  MainWindowSetupModule m_setup;

  QVector<CameraTileWidget*> m_tiles;
  QStackedWidget* m_imageStack = nullptr;
  QWidget* m_gridPage = nullptr;
  QWidget* m_gridContent = nullptr;
  QGridLayout* m_gridLayout = nullptr;
  CommandToolbarWidget* m_commandToolbar = nullptr;
  CameraStripWidget* m_cameraStrip = nullptr;
  ToolIconBarWidget* m_toolIconBar = nullptr;
  MeasurementResultsWidget* m_measurementResults = nullptr;
  ImageViewWidget* m_largeImage = nullptr;
  QLabel* m_largeTitle = nullptr;
  QLabel* m_systemStatus = nullptr;
  QLabel* m_cameraDetails = nullptr;
  CameraSetupPanelWidget* m_setupPanel = nullptr;
  QString m_setupCameraId;
  QString m_returnToSetupCameraId;
  QWidget* m_toolsContainer = nullptr;
  QVBoxLayout* m_toolsLayout = nullptr;
  QTextEdit* m_log = nullptr;
  QString m_selectedCameraId;
  CameraConfig m_selectedCamera;
  QPixmap m_selectedPreview;
  QString m_selectedImagePath;
  QTimer* m_simulationTimer = nullptr;
  std::map<QString, CameraRuntime> m_cameraRuntime;
  QHash<QString, LocalizationResult> m_lastLocalizationResults;
  QHash<QString, SurfaceLocalizationReference> m_lastSurfaceLocalizationResults;
  QHash<QString, qint64> m_lastSetupScanElapsedMs;
  QHash<QString, bool> m_cameraProcessingBusy;
  QHash<QString, int> m_cameraDroppedFrames;
  QHash<QString, int> m_cameraPendingJobs;
  bool m_machineRunning = false;
  bool m_setupDetailsVisible = false;

  MainWindowActiveDrawingRecipe m_activeDrawingRecipe = MainWindowActiveDrawingRecipe::None;
};
