#include "gui/MainWindow.h"

#include "gui/CameraTileWidget.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "config/RecipeJsonUtils.h"
#include "util/AsyncExecutor.h"
#include "util/CameraAsyncExecutor.h"
#include "simulator/SimulatorBridge.h"

#include <QDir>
#include <QFileInfo>
#include <QLayout>
#include <QMetaObject>
#include <QSettings>
#include <QVariant>

namespace
{
constexpr bool kForceGuruStartup = true;
}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
  , m_ctx{}
  , m_imaging(m_ctx)
  , m_recipes(m_ctx)
  , m_geometry(m_ctx)
  , m_surface(m_ctx)
  , m_localization(m_ctx)
  , m_cameraConfig(m_ctx)
  , m_constructedGeometry(m_ctx)
  , m_measurement(m_ctx)
  , m_thread(m_ctx)
  , m_setup(m_ctx)
{
  m_recipeManager.setRecipeId(RecipeManager::loadActiveRecipeId());

  QString error;
  if (!m_translations.loadLanguage("it", &error))
  {
    m_translations.loadLanguage("en");
  }

  {
    QSettings settings;
    if (kForceGuruStartup)
    {
      settings.setValue("access/startAsGuru", true);
    }
    if (settings.value("access/startAsGuru", true).toBool())
    {
      AccessUser startupUser;
      startupUser.id = "guru-startup";
      startupUser.displayName = accessRoleLabel(AccessRole::Guru);
      startupUser.role = AccessRole::Guru;
      startupUser.enabled = true;
      m_accessSession.setUser(startupUser);
    }
  }

  buildUi();
  bindModules();

  QString simulatorError;
  if (!SimulatorBridge::instance().start(&simulatorError))
  {
    appendLog(simulatorError);
  }
  else
  {
    appendLog(QString("Server simulatore pronto: %1").arg(SimulatorBridge::instance().serverName()));
    SimulatorBridge::instance().setFrameAvailableHandler([this](const QString& channel) {
      QMetaObject::invokeMethod(this, [this, channel]() {
        handleSimulatorFrameAvailable(channel);
      });
    });
    SimulatorBridge::instance().setSampleAvailableHandler([this](const SimulatorFrame& frame) {
      QMetaObject::invokeMethod(this, [this, frame]() {
        handleSimulatorSampleAvailable(frame);
      });
    });
    SimulatorBridge::instance().setRecipeRequestHandler(
      [this](const QString& recipeId, QString* errorMessage) {
        bool accepted = false;
        QString error;
        QMetaObject::invokeMethod(
          this,
          [this, recipeId, &accepted, &error]() {
            const QString recipePath = QDir(RecipeManager::recipesRootPath())
              .filePath(recipeId + "/recipe.json");
            if (!QFileInfo::exists(recipePath))
            {
              error = "Ricetta non trovata: " + recipeId;
              return;
            }
            m_recipes.setActiveRecipe(recipeId);
            accepted = m_recipeManager.recipeId() == recipeId;
            if (!accepted)
            {
              error = "Caricamento ricetta fallito: " + recipeId;
            }
          },
          Qt::BlockingQueuedConnection);
        if (errorMessage)
        {
          *errorMessage = error;
        }
        return accepted;
      });
  }

  AsyncExecutor::setDefaultMaxThreadsToHardware();
  {
    QSettings settings;
    const QVariant v = settings.value("system/maxThreads", QVariant());
    if (v.isValid())
    {
      const int saved = v.toInt();
      if (saved <= 0)
      {
        AsyncExecutor::setDefaultMaxThreadsToHardware();
      }
      else
      {
        AsyncExecutor::setMaxThreads(saved);
      }
    }
  }

  AsyncExecutor::setMetricsHandler(nullptr);
  syncAsyncMetricsLogging();

  {
    QSettings settings;
    const bool detailedLogEnabled = settings.value("system/detailedLogEnabled", false).toBool();
    m_setupDetailsVisible = settings.value("ui/setupDetailsVisible", false).toBool();
    if (detailedLogEnabled)
    {
      setDetailedLogEnabled(true);
    }
    else
    {
      syncAsyncMetricsLogging();
    }
  }

  loadConfiguration();
}

void MainWindow::loadConfiguration()
{
  QString error;
  const QString configPath = RecipeJsonUtils::appPath("config/cameras.json");

  if (!m_config.load(configPath, &error))
  {
    m_systemStatus->setText(trText("status.configError"));
    if (m_commandToolbar)
    {
      m_commandToolbar->setStatusText(m_systemStatus->text());
    }
    appendLog(error, true);
    return;
  }

  buildMenu();

  while (QLayoutItem* item = m_gridLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }
    delete item;
  }

  m_tiles.clear();
  resetMeasurementStatistics();
  const QVector<CameraConfig> cameras = m_config.activeCameras();
  m_recipes.ensureRecipeCameraFolders();
  if (m_cameraStrip)
  {
    m_cameraStrip->setCameras(cameras);
    m_cameraStrip->setSelectedCamera({});
  }

  for (int i = 0; i < cameras.size(); ++i)
  {
    const QPixmap preview = m_imaging.loadCameraPreview(cameras[i]);
    auto* tile = new CameraTileWidget(cameras[i], m_gridContent);
    tile->setPreview(preview);
    tile->setClickHandler([this](const CameraConfig& camera) { selectCamera(camera); });
    m_gridLayout->addWidget(tile, i / 4, i % 4);
    m_tiles.append(tile);
  }

  m_systemStatus->setText(QString("%1 | %2 %3")
                            .arg(trText("status.systemReady"))
                            .arg(cameras.size())
                            .arg(trText("status.activeCameras")));
  if (m_commandToolbar)
  {
    m_commandToolbar->setStatusText(m_systemStatus->text());
    m_commandToolbar->setRecipeText(QString("%1: %2").arg(trText("labels.recipe"), m_recipeManager.recipeId()));
  }
  appendLog(QString("%1: %2 %3 %4 %5")
              .arg(trText("log.configLoaded"))
              .arg(cameras.size())
              .arg(trText("status.activeCameras"))
              .arg(trText("labels.max"))
              .arg(m_config.maxCameras()));

  if (!cameras.isEmpty())
  {
    updateControlPanel(nullptr);
  }

  showGridView();
}

void MainWindow::bindModules()
{
  m_ctx.window = this;
  m_ctx.config = &m_config;
  m_ctx.recipes = &m_recipeManager;
  m_ctx.translations = &m_translations;
  m_ctx.tiles = &m_tiles;
  m_ctx.imageStack = m_imageStack;
  m_ctx.gridPage = m_gridPage;
  m_ctx.gridContent = m_gridContent;
  m_ctx.gridLayout = m_gridLayout;
  m_ctx.largeImage = m_largeImage;
  m_ctx.largeTitle = m_largeTitle;
  m_ctx.systemStatus = m_systemStatus;
  m_ctx.cameraDetails = m_cameraDetails;
  m_ctx.setupPanel = &m_setupPanel;
  m_ctx.setupCameraId = &m_setupCameraId;
  m_ctx.setupDetailsVisible = &m_setupDetailsVisible;
  m_ctx.returnToSetupCameraId = &m_returnToSetupCameraId;
  m_ctx.toolsContainer = m_toolsContainer;
  m_ctx.toolsLayout = m_toolsLayout;
  m_ctx.log = m_log;
  m_ctx.selectedCameraId = &m_selectedCameraId;
  m_ctx.selectedCamera = &m_selectedCamera;
  m_ctx.selectedPreview = &m_selectedPreview;
  m_ctx.selectedImagePath = &m_selectedImagePath;
  m_ctx.simulationTimer = m_simulationTimer;
  m_ctx.cameraRuntime = &m_cameraRuntime;
  m_ctx.lastLocalizationResults = &m_lastLocalizationResults;
  m_ctx.lastSurfaceLocalizationResults = &m_lastSurfaceLocalizationResults;
  m_ctx.lastSetupScanElapsedMs = &m_lastSetupScanElapsedMs;
  m_ctx.cameraProcessingBusy = &m_cameraProcessingBusy;
  m_ctx.cameraDroppedFrames = &m_cameraDroppedFrames;
  m_ctx.cameraPendingJobs = &m_cameraPendingJobs;
  m_ctx.machineRunning = &m_machineRunning;
  m_ctx.activeDrawingRecipe = &m_activeDrawingRecipe;

  m_ctx.geometry = &m_geometry;
  m_ctx.surface = &m_surface;
  m_ctx.localization = &m_localization;
  m_ctx.imaging = &m_imaging;
  m_ctx.recipeModule = &m_recipes;
  m_ctx.setup = &m_setup;
  m_ctx.cameraConfig = &m_cameraConfig;
  m_ctx.constructedGeometry = &m_constructedGeometry;
  m_ctx.measurement = &m_measurement;
  m_ctx.thread = &m_thread;

  m_ctx.syncThreadExtractionRoiOverlay = [this](const CameraConfig& camera) {
    m_thread.syncExtractionRoiOverlay(camera);
  };
  m_ctx.refreshThreadProfileOverlay = [this](const CameraConfig& camera) {
    m_thread.refreshThreadProfileOverlay(camera);
  };

  m_ctx.trText = [this](const QString& key) { return trText(key); };
  m_ctx.isDetailedLogEnabled = [this]() { return isDetailedLogEnabled(); };
  m_ctx.appendLog = [this](const QString& message) { appendLog(message); };
  m_ctx.updateLargePreview = [this]() { updateLargePreview(); };
  m_ctx.updateMeasurementResults = [this]() { scheduleMeasurementResultsUpdate(); };
  m_ctx.updateMeasurementStatistics = [this](const QString& cameraId) {
    updateMeasurementStatistics(cameraId, true);
  };
  m_ctx.resetMeasurementStatistics = [this]() { resetMeasurementStatistics(); };
  m_ctx.reloadCameraReferenceImage = [this](const CameraConfig& camera) { m_imaging.reloadCameraReferenceImage(camera); };
  m_ctx.updateControlPanel = [this](const CameraConfig* camera) { updateControlPanel(camera); };
  m_ctx.clearToolPanel = [this]() { clearToolPanel(); };
  m_ctx.deactivateImageDrawingTools = [this]() { deactivateImageDrawingTools(); };
  m_ctx.showCameraToolList = [this](const CameraConfig& camera) { showCameraToolList(camera); };
  m_ctx.selectCamera = [this](const CameraConfig& camera) { selectCamera(camera); };
  m_ctx.showGridView = [this]() { showGridView(); };
  m_ctx.updateRecipeText = [this](const QString& recipeId) {
    if (m_commandToolbar)
    {
      m_commandToolbar->setRecipeText(QString("%1: %2").arg(trText("labels.recipe"), recipeId));
    }
  };
  m_ctx.refreshSelectedCameraRecipeData = [this]() { m_recipes.refreshSelectedCameraRecipeData(); };
  m_ctx.ensureRecipeCameraFolders = [this]() { m_recipes.ensureRecipeCameraFolders(); };
  m_ctx.optionalToolVisible = [this](const QString& toolId) {
    if (m_accessSession.role() == AccessRole::Guru)
    {
      return true;
    }
    QSettings settings;
    return settings.value(QString("access/tools/%1").arg(toolId), true).toBool();
  };
  m_ctx.showLocalizationStrategyList = [this](const CameraConfig& camera) { showLocalizationStrategyList(camera); };
  m_ctx.loadConfiguration = [this]() { loadConfiguration(); };
  m_ctx.incPendingJobs = [this](const QString& cameraId) { incPendingJobs(cameraId); };
  m_ctx.decPendingJobs = [this](const QString& cameraId) { decPendingJobs(cameraId); };
  m_ctx.notifyProductionPieceCompleted = [this](const QString& cameraId) {
    notifyProductionPieceCompleted(cameraId);
  };
  m_ctx.publishSimulatorResult = [this](const QString& cameraId) { publishSimulatorResult(cameraId); };
}
