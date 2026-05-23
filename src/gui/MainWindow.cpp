#include "gui/MainWindow.h"

#include "gui/CameraTileWidget.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "config/RecipeJsonUtils.h"
#include "util/AsyncExecutor.h"

#include <QDir>
#include <QLayout>
#include <QSettings>
#include <QVariant>

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
  , m_setup(m_ctx)
{
  m_recipeManager.setRecipeId(RecipeManager::loadActiveRecipeId());

  QString error;
  if (!m_translations.loadLanguage("it", &error))
  {
    m_translations.loadLanguage("en");
  }

  buildUi();
  bindModules();

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

  AsyncExecutor::setMetricsHandler([this](const QString& name, qint64 ms) {
    appendLog(name.isEmpty()
      ? QString("metric: %1 ms").arg(ms)
      : QString("metric %1: %2 ms").arg(name).arg(ms));
  });

  {
    QSettings settings;
    const bool detailedLogEnabled = settings.value("system/detailedLogEnabled", false).toBool();
    m_setupDetailsVisible = settings.value("ui/setupDetailsVisible", false).toBool();
    if (detailedLogEnabled)
    {
      setDetailedLogEnabled(true);
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
    appendLog(error);
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

  m_ctx.trText = [this](const QString& key) { return trText(key); };
  m_ctx.appendLog = [this](const QString& message) { appendLog(message); };
  m_ctx.updateLargePreview = [this]() { updateLargePreview(); };
  m_ctx.updateMeasurementResults = [this]() { updateMeasurementResults(); };
  m_ctx.reloadCameraReferenceImage = [this](const CameraConfig& camera) { m_imaging.reloadCameraReferenceImage(camera); };
  m_ctx.updateControlPanel = [this](const CameraConfig* camera) { updateControlPanel(camera); };
  m_ctx.clearToolPanel = [this]() { clearToolPanel(); };
  m_ctx.deactivateImageDrawingTools = [this]() { deactivateImageDrawingTools(); };
  m_ctx.showCameraToolList = [this](const CameraConfig& camera) { showCameraToolList(camera); };
  m_ctx.selectCamera = [this](const CameraConfig& camera) { selectCamera(camera); };
  m_ctx.showGridView = [this]() { showGridView(); };
  m_ctx.refreshSelectedCameraRecipeData = [this]() { m_recipes.refreshSelectedCameraRecipeData(); };
  m_ctx.ensureRecipeCameraFolders = [this]() { m_recipes.ensureRecipeCameraFolders(); };
  m_ctx.showLocalizationStrategyList = [this](const CameraConfig& camera) { showLocalizationStrategyList(camera); };
  m_ctx.loadConfiguration = [this]() { loadConfiguration(); };
  m_ctx.incPendingJobs = [this](const QString& cameraId) { incPendingJobs(cameraId); };
  m_ctx.decPendingJobs = [this](const QString& cameraId) { decPendingJobs(cameraId); };
}
