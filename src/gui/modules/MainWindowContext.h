#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "config/TranslationManager.h"
#include "gui/CameraSetupPanelWidget.h"
#include "gui/CameraTileWidget.h"
#include "gui/ImageViewWidget.h"
#include "gui/MetricsPanelWidget.h"
#include "processing/LocalizationProcessor.h"
#include "processing/SurfaceDefectProcessor.h"
#include "runtime/CameraRuntime.h"

#include <QHash>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <functional>
#include <map>

class QGridLayout;
class QLabel;
class QWidget;

class MainWindowGeometryModule;
class MainWindowSurfaceModule;
class MainWindowLocalizationModule;
class MainWindowImagingModule;
class MainWindowRecipeModule;
class MainWindowSetupModule;
class MainWindowCameraConfigModule;

enum class MainWindowActiveDrawingRecipe
{
  None,
  Localization,
  SurfaceDefects,
  Geometry
};

struct MainWindowContext
{
  QMainWindow* window = nullptr;

  AppConfig* config = nullptr;
  RecipeManager* recipes = nullptr;
  TranslationManager* translations = nullptr;

  QVector<CameraTileWidget*>* tiles = nullptr;
  QStackedWidget* imageStack = nullptr;
  QWidget* gridPage = nullptr;
  QWidget* gridContent = nullptr;
  QGridLayout* gridLayout = nullptr;
  ImageViewWidget* largeImage = nullptr;
  QLabel* largeTitle = nullptr;
  QLabel* systemStatus = nullptr;
  QLabel* cameraDetails = nullptr;
  CameraSetupPanelWidget** setupPanel = nullptr;
  QString* setupCameraId = nullptr;
  QWidget* toolsContainer = nullptr;
  QVBoxLayout* toolsLayout = nullptr;
  QTextEdit* log = nullptr;
  MetricsPanelWidget* metricsPanel = nullptr;

  QString* selectedCameraId = nullptr;
  CameraConfig* selectedCamera = nullptr;
  QPixmap* selectedPreview = nullptr;
  QString* selectedImagePath = nullptr;

  QTimer* simulationTimer = nullptr;
  std::map<QString, CameraRuntime>* cameraRuntime = nullptr;
  QHash<QString, LocalizationResult>* lastLocalizationResults = nullptr;
  QHash<QString, SurfaceLocalizationReference>* lastSurfaceLocalizationResults = nullptr;
  QHash<QString, qint64>* lastSetupScanElapsedMs = nullptr;
  QHash<QString, bool>* cameraProcessingBusy = nullptr;
  QHash<QString, int>* cameraDroppedFrames = nullptr;
  QHash<QString, int>* cameraPendingJobs = nullptr;

  MainWindowActiveDrawingRecipe* activeDrawingRecipe = nullptr;

  MainWindowGeometryModule* geometry = nullptr;
  MainWindowSurfaceModule* surface = nullptr;
  MainWindowLocalizationModule* localization = nullptr;
  MainWindowImagingModule* imaging = nullptr;
  MainWindowRecipeModule* recipeModule = nullptr;
  MainWindowSetupModule* setup = nullptr;
  MainWindowCameraConfigModule* cameraConfig = nullptr;

  std::function<QString(const QString&)> trText;
  std::function<void(const CameraConfig&)> showLocalizationStrategyList;
  std::function<void()> loadConfiguration;
  std::function<void(const QString&)> incPendingJobs;
  std::function<void(const QString&)> decPendingJobs;
  std::function<void(const QString&)> appendLog;
  std::function<void()> updateLargePreview;
  std::function<void(const CameraConfig&)> reloadCameraReferenceImage;
  std::function<void(const CameraConfig*)> updateControlPanel;
  std::function<void()> clearToolPanel;
  std::function<void()> deactivateImageDrawingTools;
  std::function<void(const CameraConfig&)> showCameraToolList;
  std::function<void(const CameraConfig&)> selectCamera;
  std::function<void()> showGridView;
  std::function<void()> refreshSelectedCameraRecipeData;
  std::function<void()> ensureRecipeCameraFolders;
};
