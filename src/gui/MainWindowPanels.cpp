#include "gui/MainWindow.h"

#include "gui/IconCatalog.h"
#include "gui/SurfaceLocalizationStrategies.h"
#include "gui/ToolCatalog.h"
#include "gui/TouchIconButton.h"
#include "simulator/SimulatorBridge.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QSettings>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  clearToolPanel();

  if (!camera)
  {
    if (m_measurementResults)
    {
      m_measurementResults->show();
    }
    m_cameraDetails->setText(trText("labels.productionOverviewDetails"));
    if (m_toolIconBar)
    {
      m_toolIconBar->setTools({});
    }
    const QVector<CameraConfig> activeCameras = m_config.activeCameras();
    int busyCount = 0;
    for (const CameraConfig& activeCamera : activeCameras)
    {
      if (m_cameraProcessingBusy.value(activeCamera.id, false))
      {
        ++busyCount;
      }
    }

    auto* title = new QLabel(trText("labels.productionStats"), m_toolsContainer);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    m_toolsLayout->addWidget(title);

    auto* stats = new QLabel(
      QString("%1: %2\n%3: %4\n%5: %6\n%7: %8")
        .arg(trText("labels.activeCamerasCount"))
        .arg(activeCameras.size())
        .arg(trText("labels.busyCamerasCount"))
        .arg(busyCount)
        .arg(trText("labels.readyCamerasCount"))
        .arg(qMax(0, activeCameras.size() - busyCount))
        .arg(trText("labels.machineMode"))
        .arg(m_machineRunning ? "START" : "STOP"),
      m_toolsContainer);
    stats->setObjectName("toolPanelNote");
    stats->setWordWrap(true);
    m_toolsLayout->addWidget(stats);

    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(QString("%1 %2 | %3")
    .arg(trText("labels.camera"))
    .arg(camera->slot)
    .arg(camera->displayName));

  if (m_measurementResults)
  {
    m_measurementResults->setVisible(m_machineRunning);
  }

  if (m_machineRunning)
  {
    if (m_toolIconBar)
    {
      m_toolIconBar->setTools({});
    }
    auto* title = new QLabel(trText("labels.cameraMonitoring"), m_toolsContainer);
    title->setObjectName("toolPanelTitle");
    title->setWordWrap(true);
    m_toolsLayout->addWidget(title);
    auto* note = new QLabel(trText("labels.machineStartReadOnly"), m_toolsContainer);
    note->setObjectName("toolPanelNote");
    note->setWordWrap(true);
    m_toolsLayout->addWidget(note);
    const bool busy = m_cameraProcessingBusy.value(camera->id, false);
    const QString channel = camera->simulatorChannel.isEmpty() ? camera->id : camera->simulatorChannel;
    const int pendingQueue = camera->type == "simulator"
      ? SimulatorBridge::instance().queueSize(channel)
      : 0;
    auto* stats = new QLabel(
      QString("%1: %2\n%3: %4\n%5: %6 ms")
        .arg(trText("labels.cameraState"))
        .arg(busy ? "BUSY" : "READY")
        .arg(trText("labels.pendingFrames"))
        .arg(m_cameraPendingJobs.value(camera->id, 0) + pendingQueue)
        .arg(trText("labels.lastScan"))
        .arg(m_lastSetupScanElapsedMs.value(camera->id, 0)),
      m_toolsContainer);
    stats->setObjectName("toolPanelNote");
    stats->setWordWrap(true);
    m_toolsLayout->addWidget(stats);
    m_toolsLayout->addStretch(1);
    return;
  }

  showCameraToolList(*camera);
}

void MainWindow::updateMeasurementResults()
{
  if (!m_measurementResults)
  {
    return;
  }

  const AccessRole role = m_accessSession.role();
  m_measurementResults->setShowScanTime(
    role == AccessRole::Administrator || role == AccessRole::Guru);

  if (m_selectedCameraId.isEmpty())
  {
    QVector<CameraMeasurementResultRow> rows;
    for (const CameraConfig& camera : m_config.activeCameras())
    {
      const auto runtimeIt = m_cameraRuntime.find(camera.id);
      if (runtimeIt == m_cameraRuntime.end())
      {
        continue;
      }

      const QVector<MeasurementResult>& measurements = runtimeIt->second.geometries().measurements;
      const PartPose& pose = runtimeIt->second.currentPose();
      for (CameraTileWidget* tile : m_tiles)
      {
        if (tile->camera().id == camera.id)
        {
          tile->setResultText(
            pose.valid
              ? QString("X %1  Y %2  A %3°")
                  .arg(pose.origin.x, 0, 'f', 2)
                  .arg(pose.origin.y, 0, 'f', 2)
                  .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 2)
              : QString("In attesa"));
          break;
        }
      }
      for (const MeasurementResult& measurement : measurements)
      {
        rows.append({
          camera.id,
          measurement,
          m_lastSetupScanElapsedMs.value(camera.id, -1)
        });
      }
    }
    m_measurementResults->setAllCameraMeasurements(rows);
    return;
  }

  m_measurementResults->setMeasurements(
    m_selectedCameraId,
    m_cameraRuntime[m_selectedCameraId].geometries().measurements,
    m_lastSetupScanElapsedMs.value(m_selectedCameraId, -1));
}

void MainWindow::updateCameraStripStatus(const QString& cameraId)
{
  if (!m_cameraStrip || cameraId.isEmpty())
  {
    return;
  }

  const bool busy = m_cameraProcessingBusy.value(cameraId, false);
  m_cameraStrip->setCameraBusy(cameraId, busy);
  if (!busy)
  {
    m_cameraStrip->setCameraResult(cameraId, "READY");
  }
}

void MainWindow::deactivateImageDrawingTools()
{
  if (!m_largeImage)
  {
    return;
  }

  m_largeImage->setRoiDrawingEnabled(false);
  m_largeImage->setExclusionDrawingEnabled(false);
  m_largeImage->setOuterCircleDrawingEnabled(false);
  m_largeImage->setInnerCircleDrawingEnabled(false);
  m_largeImage->setCircleBandEditingEnabled(false);
  m_largeImage->clearDetectedCircle();
  m_largeImage->setThreePointCircleDrawingEnabled(false);
  m_largeImage->setThreePointArcDrawingEnabled(false);
  m_largeImage->setGeometryPointPickingEnabled(false);
  m_largeImage->setGeometryOverlayPointEditingEnabled(false);
  m_largeImage->setTwoPointLineDrawingEnabled(false);
  m_activeDrawingRecipe = MainWindowActiveDrawingRecipe::None;
  m_surface.setCircleTarget(MainWindowSurfaceModule::CircleTarget::None);
  m_geometry.setDrawingTarget(MainWindowGeometryModule::DrawingTarget::None);
}

void MainWindow::showCameraToolList(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  m_returnToSetupCameraId.clear();
  clearToolPanel();

  QVector<ToolIconDefinition> tools;
  QSet<QString> addedTools;
  auto addTool = [this, &tools, &addedTools](const QString& id) {
    if (addedTools.contains(id))
    {
      return;
    }
    addedTools.insert(id);
    tools.append({id, ToolCatalog::label(id, m_translations), id});
  };

  addTool("setup");
  addTool("localization");
  addTool("geometries");
  addTool("constructedGeometries");
  addTool("measurements");
  const bool hasVisibleAiTool =
    m_accessSession.role() == AccessRole::Guru ||
    QSettings().value("access/tools/aiClassification", true).toBool() ||
    QSettings().value("access/tools/aiAnomaly", true).toBool() ||
    QSettings().value("access/tools/aiSegmentation", true).toBool();
  if (hasVisibleAiTool)
  {
    addTool("ai");
  }

  for (const QString& tool : camera.profile.guiTools)
  {
    if ((tool == "aiClassification" || tool == "aiAnomaly" || tool == "aiSegmentation") &&
        m_accessSession.role() != AccessRole::Guru &&
        !QSettings().value(QString("access/tools/%1").arg(tool), true).toBool())
    {
      continue;
    }
    if (tool == "localization" ||
        tool == "surfaceLocalization" ||
        tool == "geometries" ||
        tool == "measurements" ||
        tool == "tolerances")
    {
      continue;
    }
    addTool(tool);
  }

  if (m_toolIconBar)
  {
    m_toolIconBar->setTools(tools);
    m_toolIconBar->setActiveTool({});
  }

  auto* note = new QLabel(trText("labels.noToolSelected"), m_toolsContainer);
  note->setWordWrap(true);
  note->setObjectName("toolPanelNote");
  m_toolsLayout->addWidget(note);
  m_toolsLayout->addStretch(1);
}

void MainWindow::showLocalizationStrategyList(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();

  auto* panel = new QWidget(m_toolsContainer);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* title = new QLabel(trText("tools.localization") + " | " + QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot), panel);
  title->setObjectName("toolPanelTitle");
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* strategyTitle = new QLabel(trText("groups.localizationStrategies"), panel);
  strategyTitle->setObjectName("toolPanelNote");
  strategyTitle->setWordWrap(true);
  layout->addWidget(strategyTitle);

  auto* strategyGrid = new QWidget(panel);
  auto* strategyLayout = new QGridLayout(strategyGrid);
  strategyLayout->setContentsMargins(0, 0, 0, 0);
  strategyLayout->setSpacing(6);

  int strategyIndex = 0;
  for (const SurfaceLocalizationStrategyDefinition& strategy : SurfaceLocalizationStrategies::all(m_translations))
  {
    if (strategy.id == "aiYolo" &&
        m_accessSession.role() != AccessRole::Guru &&
        !QSettings().value("access/tools/aiLocalization", true).toBool())
    {
      continue;
    }
    const QString iconId = strategy.id == "threshold" ? "surfaceCircleThreshold" :
      (strategy.id == "edge" ? "surfaceCircleEdge" :
       (strategy.id == "two_circles_axis" ? "circleCircleIntersection" :
        (strategy.id == "edgePca" ? "surfacePca" :
         (strategy.id == "massPca" ? "surfaceSearchRoi" :
          (strategy.id == "model" ? "surfaceModel" : "aiModel")))));
    auto* button = createTouchIconButton(iconId, strategy.label, strategyGrid);
    connect(button, &QPushButton::clicked, this, [this, camera, strategy]() {
      if (strategy.id == "aiYolo")
      {
        m_setup.showAiLocalizationPanel(camera);
        return;
      }
      m_surface.showSurfaceLocalizationStrategyPanel(camera, strategy.id);
    });
    strategyLayout->addWidget(button, strategyIndex / 4, strategyIndex % 4);
    ++strategyIndex;
  }
  layout->addWidget(strategyGrid);

  auto* backButton = createTouchIconButton("back", trText("commands.backToCameraTools"), panel);
  connect(backButton, &QPushButton::clicked, this, [this, camera]() {
    showCameraToolList(camera);
    appendLog(trText("log.backToCameraTools") + ": " + camera.id);
  });
  layout->addWidget(backButton);
  layout->addStretch(1);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + trText("tools.localization"));
}

void MainWindow::clearToolPanel()
{
  m_setupPanel = nullptr;
  m_setupCameraId.clear();
  while (QLayoutItem* item = m_toolsLayout->takeAt(0))
  {
    if (QWidget* widget = item->widget())
    {
      widget->deleteLater();
    }

    delete item;
  }

  addGrabToggleToToolPanel();
}

void MainWindow::addGrabToggleToToolPanel()
{
  if (m_selectedCameraId.isEmpty() || m_machineRunning || !m_toolsLayout || !m_toolsContainer)
  {
    return;
  }

  const auto runtimeIt = m_cameraRuntime.find(m_selectedCameraId);
  const bool running = runtimeIt != m_cameraRuntime.end() && runtimeIt->second.running();
  const QString label = running ? trText("commands.stop") : trText("commands.start");

  auto* bar = new QWidget(m_toolsContainer);
  auto* layout = new QHBoxLayout(bar);
  layout->setContentsMargins(0, 0, 0, 4);
  layout->setSpacing(8);

  auto* button = createTouchIconButton(running ? "stop" : "start", label, bar);
  auto* text = new QLabel(QString("Grab: %1").arg(label), bar);
  text->setObjectName("toolPanelNote");
  text->setWordWrap(true);
  auto* restoreSampleButton = createTouchIconButton(
    "reload",
    trText("actions.restoreSampleImage"),
    bar);

  connect(button, &QPushButton::clicked, this, [this, button, text]() {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_cameraRuntime[m_selectedCameraId].running())
    {
      m_setup.stopCameraSimulation(m_selectedCamera, false);
    }
    else
    {
      m_setup.startCameraSimulation(m_selectedCamera, false);
    }

    const bool isRunning = m_cameraRuntime[m_selectedCameraId].running();
    const QString nextLabel = isRunning ? trText("commands.stop") : trText("commands.start");
    button->setIcon(IconCatalog::icon(isRunning ? "stop" : "start"));
    button->setToolTip(nextLabel);
    button->setAccessibleName(nextLabel);
    text->setText(QString("Grab: %1").arg(nextLabel));
  });

  layout->addWidget(button);
  layout->addWidget(restoreSampleButton);
  layout->addWidget(text, 1);
  m_toolsLayout->addWidget(bar);

  connect(restoreSampleButton, &QPushButton::clicked, this, [this, button, text]() {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    m_imaging.restoreSampleWorkspace(m_selectedCamera);
    const QString startLabel = trText("commands.start");
    button->setIcon(IconCatalog::icon("start"));
    button->setToolTip(startLabel);
    button->setAccessibleName(startLabel);
    text->setText(QString("Grab: %1").arg(startLabel));
    appendLog(trText("actions.restoreSampleImage") + ": " + m_selectedCameraId);
  });
}

