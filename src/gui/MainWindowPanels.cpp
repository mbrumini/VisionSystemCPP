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

#include <algorithm>
#include <cmath>

namespace
{
QString measurementResultKey(const QString& type, const QString& sourceAId, const QString& sourceBId)
{
  return QString("%1|%2|%3").arg(type, sourceAId, sourceBId);
}

MeasurementResult failedMeasurementRow(const MeasurementRecipeConfig& config)
{
  MeasurementResult result;
  result.id = config.id;
  result.alias = config.alias;
  result.type = config.type;
  result.sourceAId = config.sourceAId;
  result.sourceBId = config.sourceBId;
  result.valid = false;
  result.judgement = "N/D";
  result.unit = config.unit.isEmpty() ? "px" : config.unit;
  if (config.type == "line_line_angle" || config.type == "thread_phase")
  {
    result.unit = "deg";
  }
  result.nominal = config.nominal;
  result.min = config.min;
  result.max = config.max;
  result.hasNominal = config.hasNominal;
  result.hasMin = config.hasMin;
  result.hasMax = config.hasMax;
  return result;
}

const MeasurementResult* findRuntimeMeasurement(const QVector<MeasurementResult>& measurements,
                                                const MeasurementRecipeConfig& config)
{
  const QString key = measurementResultKey(config.type, config.sourceAId, config.sourceBId);
  const MeasurementResult* fallback = nullptr;
  for (const MeasurementResult& measurement : measurements)
  {
    if (measurementResultKey(measurement.type, measurement.sourceAId, measurement.sourceBId) != key)
    {
      continue;
    }

    if (measurement.valid)
    {
      return &measurement;
    }

    if (!fallback)
    {
      fallback = &measurement;
    }
  }
  return fallback;
}

const MeasurementResult* findRuntimeMeasurementByType(const QVector<MeasurementResult>& measurements,
                                                        const QString& type)
{
  for (const MeasurementResult& measurement : measurements)
  {
    if (measurement.type == type)
    {
      return &measurement;
    }
  }
  return nullptr;
}

void appendThreadMeasurementRows(const RecipeManager& recipes,
                               const QString& cameraId,
                               const QVector<MeasurementResult>& runtimeMeasurements,
                               QVector<MeasurementResult>& rows)
{
  const ThreadInspectionSettings threadSettings = recipes.loadThreadInspectionSettings(cameraId);
  if (!threadSettings.enabled || !threadSettings.hasExtractionRoi)
  {
    return;
  }

  const QStringList types = {
    QStringLiteral("thread_major_diameter"),
    QStringLiteral("thread_minor_diameter"),
    QStringLiteral("thread_pitch"),
    QStringLiteral("thread_phase")
  };
  const ThreadMeasurementLimits* limits[] = {
    &threadSettings.majorDiameter,
    &threadSettings.minorDiameter,
    &threadSettings.pitchLength,
    &threadSettings.phaseOffset
  };
  for (int index = 0; index < types.size(); ++index)
  {
    if (!limits[index]->enabled)
    {
      continue;
    }

    const MeasurementResult* runtime = findRuntimeMeasurementByType(runtimeMeasurements, types[index]);
    if (runtime)
    {
      rows.append(*runtime);
    }
  }

  if (const MeasurementResult* runtime =
        findRuntimeMeasurementByType(runtimeMeasurements, QStringLiteral("thread_pitch_diameter")))
  {
    rows.append(*runtime);
  }
}

QVector<MeasurementResult> mergedMeasurementRows(const RecipeManager& recipes,
                                                 const QString& cameraId,
                                                 const QVector<MeasurementResult>& runtimeMeasurements)
{
  QVector<MeasurementResult> rows;
  for (const MeasurementRecipeConfig& config : recipes.loadMeasurements(cameraId))
  {
    if (!config.enabled)
    {
      continue;
    }

    const MeasurementResult* runtime = findRuntimeMeasurement(runtimeMeasurements, config);
    rows.append(runtime ? *runtime : failedMeasurementRow(config));
  }
  appendThreadMeasurementRows(recipes, cameraId, runtimeMeasurements, rows);
  return rows;
}

const QVector<MeasurementResult>& measurementsForDisplay(
  const QString& cameraId,
  const QVector<MeasurementResult>& live,
  QHash<QString, QVector<MeasurementResult>>& publishedCache)
{
  if (!live.isEmpty())
  {
    publishedCache[cameraId] = live;
    return live;
  }

  const auto cached = publishedCache.constFind(cameraId);
  if (cached != publishedCache.constEnd() && !cached->isEmpty())
  {
    return cached.value();
  }

  return live;
}

QString measurementStatisticKey(const QString& cameraId, const MeasurementResult& measurement)
{
  return QString("%1|%2|%3|%4")
    .arg(cameraId, measurement.type, measurement.sourceAId, measurement.sourceBId);
}

QString measurementDisplayName(const MeasurementResult& measurement)
{
  return measurement.alias.trimmed().isEmpty() ? measurement.id : measurement.alias.trimmed();
}

bool measurementValueForStatistics(const MeasurementResult& measurement, double& value, QString& unit)
{
  if (!measurement.valid)
  {
    return false;
  }
  if (measurement.hasRealValue)
  {
    value = measurement.valueReal;
    unit = measurement.unit;
    return true;
  }
  value = measurement.valuePixels;
  unit = "px";
  return true;
}

bool isMeasurementStatisticOutlier(const MeasurementResult& measurement, double value)
{
  if (!measurement.hasMin || !measurement.hasMax || measurement.max <= measurement.min)
  {
    return false;
  }

  const double center = measurement.hasNominal
    ? measurement.nominal
    : (measurement.min + measurement.max) * 0.5;
  const double lowLimit = center + (measurement.min - center) * 3.0;
  const double highLimit = center + (measurement.max - center) * 3.0;
  return value < std::min(lowLimit, highLimit) || value > std::max(lowLimit, highLimit);
}

QString compactMeasurementValue(double value, const QString& unit)
{
  return QString("%1 %2").arg(value, 0, 'f', unit == "px" ? 1 : 3).arg(unit);
}
}

void MainWindow::resetMeasurementStatistics()
{
  m_measurementStatistics.clear();
}

void MainWindow::updateMeasurementStatistics(const QString& cameraId, bool accumulate)
{
  auto runtimeIt = m_cameraRuntime.find(cameraId);
  if (runtimeIt == m_cameraRuntime.end())
  {
    return;
  }

  if (!accumulate)
  {
    return;
  }

  constexpr int kRecentMeasurementWindow = 1000;
  const QVector<MeasurementResult>& measurements = runtimeIt->second.geometries().measurements;
  const QVector<MeasurementResult> rows = mergedMeasurementRows(m_recipeManager, cameraId, measurements);

  for (const MeasurementResult& measurement : rows)
  {
    double value = 0.0;
    QString unit;
    const bool hasCurrentValue = measurementValueForStatistics(measurement, value, unit);
    const bool acceptForStatistics =
      hasCurrentValue && !isMeasurementStatisticOutlier(measurement, value);
    if (!acceptForStatistics)
    {
      continue;
    }

    MeasurementStatisticState& state = m_measurementStatistics[measurementStatisticKey(cameraId, measurement)];
    state.unit = unit;
    state.recentValues.enqueue(value);
    state.recentSum += value;
    while (state.recentValues.size() > kRecentMeasurementWindow)
    {
      state.recentSum -= state.recentValues.dequeue();
    }
    if (!state.hasValue)
    {
      state.minimum = value;
      state.maximum = value;
      state.hasValue = true;
    }
    else
    {
      state.minimum = std::min(state.minimum, value);
      state.maximum = std::max(state.maximum, value);
    }
  }

  scheduleMeasurementResultsUpdate();
}

void MainWindow::updateControlPanel(const CameraConfig* camera)
{
  if (!camera)
  {
    if (m_productionOverviewStatsLabel)
    {
      refreshProductionOverviewPanel();
      return;
    }
  }
  else if (m_machineRunning && m_cameraMonitoringStatsLabel && camera->id == m_selectedCameraId)
  {
    refreshCameraMonitoringPanel(*camera);
    return;
  }

  clearToolPanel();
  m_productionOverviewStatsLabel = nullptr;
  m_productionOverviewThroughputLabel = nullptr;
  m_cameraMonitoringStatsLabel = nullptr;

  if (!camera)
  {
    if (m_measurementResults)
    {
      m_measurementResults->show();
      m_measurementResults->setExpanded(true);
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
    m_productionOverviewStatsLabel = stats;

    if (m_machineRunning)
    {
      auto* throughput = new QLabel(
        QString("%1: %2\n%3: %4")
          .arg(trText("labels.throughputInstant"))
          .arg(throughputOverviewInstantText())
          .arg(trText("labels.throughputAverage10s"))
          .arg(throughputOverviewAverageText()),
        m_toolsContainer);
      throughput->setObjectName("toolPanelNote");
      throughput->setWordWrap(true);
      m_toolsLayout->addWidget(throughput);
      m_productionOverviewThroughputLabel = throughput;
    }

    m_toolsLayout->addStretch(1);
    return;
  }

  m_cameraDetails->setText(QString("%1 %2 | %3")
    .arg(trText("labels.camera"))
    .arg(camera->slot)
    .arg(camera->displayName));

  if (m_measurementResults)
  {
    m_measurementResults->setVisible(false);
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
      QString("%1: %2\n%3: %4\n%5: %6 ms\n%7: %8\n%9: %10")
        .arg(trText("labels.cameraState"))
        .arg(busy ? "BUSY" : "READY")
        .arg(trText("labels.pendingFrames"))
        .arg(m_cameraPendingJobs.value(camera->id, 0) + pendingQueue)
        .arg(trText("labels.lastScan"))
        .arg(m_lastSetupScanElapsedMs.value(camera->id, 0))
        .arg(trText("labels.throughputInstant"))
        .arg(throughputInstantText(camera->id))
        .arg(trText("labels.throughputAverage10s"))
        .arg(throughputAverageText(camera->id)),
      m_toolsContainer);
    stats->setObjectName("toolPanelNote");
    stats->setWordWrap(true);
    m_toolsLayout->addWidget(stats);
    m_cameraMonitoringStatsLabel = stats;
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

      const QVector<MeasurementResult>& liveMeasurements = runtimeIt->second.geometries().measurements;
      const QVector<MeasurementResult>& measurements =
        measurementsForDisplay(camera.id, liveMeasurements, m_lastPublishedMeasurements);
      for (const MeasurementResult& measurement : mergedMeasurementRows(m_recipeManager, camera.id, measurements))
      {
        CameraMeasurementResultRow row;
        row.cameraId = camera.id;
        row.measurement = measurement;
        row.scanElapsedMs = m_lastSetupScanElapsedMs.value(camera.id, -1);

        const auto statIt = m_measurementStatistics.constFind(measurementStatisticKey(camera.id, measurement));
        if (statIt != m_measurementStatistics.constEnd() && statIt->hasValue)
        {
          row.hasStatistics = true;
          row.statisticsUnit = statIt->unit;
          row.averageValue = statIt->recentValues.isEmpty()
            ? statIt->minimum
            : statIt->recentSum / static_cast<double>(statIt->recentValues.size());
          row.minimumValue = statIt->minimum;
          row.maximumValue = statIt->maximum;
        }

        rows.append(row);
      }
    }
    m_measurementResults->setAllCameraMeasurements(rows);
    return;
  }

  const QVector<MeasurementResult>& liveMeasurements =
    m_cameraRuntime[m_selectedCameraId].geometries().measurements;
  const QVector<MeasurementResult>& measurements =
    measurementsForDisplay(m_selectedCameraId, liveMeasurements, m_lastPublishedMeasurements);
  m_measurementResults->setMeasurements(
    m_selectedCameraId,
    mergedMeasurementRows(
      m_recipeManager,
      m_selectedCameraId,
      measurements),
    m_lastSetupScanElapsedMs.value(m_selectedCameraId, -1));
}

void MainWindow::scheduleMeasurementResultsUpdate()
{
  if (!m_machineRunning || !m_selectedCameraId.isEmpty())
  {
    updateMeasurementResults();
    return;
  }

  if (m_measurementResultsUpdatePending)
  {
    return;
  }

  m_measurementResultsUpdatePending = true;
  QTimer::singleShot(0, this, [this]() {
    m_measurementResultsUpdatePending = false;
    updateMeasurementResults();
  });
}

void MainWindow::refreshProductionOverviewPanel()
{
  if (!m_productionOverviewStatsLabel)
  {
    return;
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

  m_productionOverviewStatsLabel->setText(
    QString("%1: %2\n%3: %4\n%5: %6\n%7: %8")
      .arg(trText("labels.activeCamerasCount"))
      .arg(activeCameras.size())
      .arg(trText("labels.busyCamerasCount"))
      .arg(busyCount)
      .arg(trText("labels.readyCamerasCount"))
      .arg(qMax(0, activeCameras.size() - busyCount))
      .arg(trText("labels.machineMode"))
      .arg(m_machineRunning ? "START" : "STOP"));

  if (m_productionOverviewThroughputLabel)
  {
    m_productionOverviewThroughputLabel->setText(
      QString("%1: %2\n%3: %4")
        .arg(trText("labels.throughputInstant"))
        .arg(throughputOverviewInstantText())
        .arg(trText("labels.throughputAverage10s"))
        .arg(throughputOverviewAverageText()));
  }
}

void MainWindow::refreshCameraMonitoringPanel(const CameraConfig& camera)
{
  if (!m_cameraMonitoringStatsLabel)
  {
    return;
  }

  const bool busy = m_cameraProcessingBusy.value(camera.id, false);
  const QString channel = camera.simulatorChannel.isEmpty() ? camera.id : camera.simulatorChannel;
  const int pendingQueue = camera.type == "simulator"
    ? SimulatorBridge::instance().queueSize(channel)
    : 0;
  m_cameraMonitoringStatsLabel->setText(
    QString("%1: %2\n%3: %4\n%5: %6 ms\n%7: %8\n%9: %10")
      .arg(trText("labels.cameraState"))
      .arg(busy ? "BUSY" : "READY")
      .arg(trText("labels.pendingFrames"))
      .arg(m_cameraPendingJobs.value(camera.id, 0) + pendingQueue)
      .arg(trText("labels.lastScan"))
      .arg(m_lastSetupScanElapsedMs.value(camera.id, 0))
      .arg(trText("labels.throughputInstant"))
      .arg(throughputInstantText(camera.id))
      .arg(trText("labels.throughputAverage10s"))
      .arg(throughputAverageText(camera.id)));
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
  addTool("tolerances");
  addTool("threadInspection");
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
        tool == "threadInspection" ||
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
  m_productionOverviewStatsLabel = nullptr;
  m_productionOverviewThroughputLabel = nullptr;
  m_cameraMonitoringStatsLabel = nullptr;
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

