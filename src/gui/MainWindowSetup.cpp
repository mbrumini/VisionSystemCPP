#include "MainWindow.h"

#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPushButton>

#include <opencv2/core.hpp>
void MainWindow::showCameraSetupPanel(const CameraConfig& camera)
{
  deactivateImageDrawingTools();
  clearToolPanel();
  restoreCleanGeometryImage(camera);

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  CameraSetupPanelTexts texts;
  texts.title = QString("%1 | %2").arg(trText("tools.setup"), camera.id);
  texts.details = cameraSetupDetailsText(camera);
  texts.frameInterval = trText("labels.frameInterval");
  texts.acquireSample = trText("actions.acquireSampleImage");
  texts.importSample = trText("actions.assignSampleImage");
  texts.importTests = trText("actions.assignTestImages");
  texts.assignImageFolder = trText("actions.assignImageFolder");
  texts.start = trText("commands.start");
  texts.stop = trText("commands.stop");
  texts.nextFrame = trText("actions.nextFrame");
  texts.back = trText("commands.backToCameraTools");

  auto* panel = new CameraSetupPanelWidget(
    texts,
    runtime.intervalMs(),
    [this, camera](int value) {
      CameraRuntime& runtime = m_cameraRuntime[camera.id];
      runtime.setIntervalMs(value);
      if (runtime.running() && camera.id == m_selectedCameraId)
      {
        m_simulationTimer->start(runtime.intervalMs());
      }
    },
    [this, camera]() { acquireCameraSampleImage(camera); },
    [this, camera]() { configureCameraSampleImage(camera); },
    [this, camera]() { configureCameraTestImages(camera); },
    [this, camera]() { configureCameraSource(camera); },
    [this, camera]() { startCameraSimulation(camera); },
    [this, camera]() { stopCameraSimulation(camera); },
    [this, camera]() { stepCameraSimulation(camera); },
    [this, camera]() {
      stopCameraSimulation(camera);
      reloadCameraReferenceImage(camera);
      showCameraToolList(camera);
    },
    m_toolsContainer);
  m_setupPanel = panel;
  m_setupCameraId = camera.id;
  m_toolsLayout->addWidget(panel);
}

void MainWindow::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
  deactivateImageDrawingTools();

  if (toolId == "geometries")
  {
    showGeometryPanel(camera);
    return;
  }

  if (toolId == "surfaceLocalization")
  {
    showSurfaceLocalizationPanel(camera);
    return;
  }

  clearToolPanel();

  const ToolDefinition tool = ToolCatalog::tool(toolId, m_translations);
  const QString noteText = tool.id == "surfaceLocalization"
    ? trText("labels.surfaceLocalizationNote")
    : trText("labels.placeholderNote");
  auto* panel = new ToolPanelWidget(
    tool,
    QString("%1 %2").arg(trText("labels.camera")).arg(camera.slot),
    noteText,
    trText("commands.backToCameraTools"),
    [this, camera]() {
      showCameraToolList(camera);
      appendLog(trText("log.backToCameraTools") + ": " + camera.id);
    },
    [this, camera, tool](const ToolActionDefinition& action) {
      if (tool.id == "localization" && action.id == "searchRoi")
      {
        activateLocalizationRoiDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "addExclusion")
      {
        activateLocalizationExclusionDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "clearExclusions")
      {
        clearLocalizationExclusions(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "testLocalization")
      {
        testLocalization(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "lineGeometry")
      {
        activateGeometryLineDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceSearchRoi")
      {
        activateSurfaceDefectRoiDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceOuterCircle")
      {
        activateSurfaceOuterCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceInnerCircle")
      {
        activateSurfaceInnerCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceThreshold")
      {
        saveSurfaceThresholdAndPreview(camera, m_recipeManager.loadSurfaceAnnulusLocalization(camera.id).thresholdMax);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceAnnulus")
      {
        testSurfaceAnnulusLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceAddExclusion")
      {
        activateSurfaceDefectExclusionDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceClearExclusions")
      {
        clearSurfaceDefectExclusions(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceLocalization")
      {
        testSurfaceLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceStrategy")
      {
        testSurfaceLocalizationStrategy(camera);
        return;
      }

      appendLog(trText("log.placeholder") + ": " + tool.label + " -> " + action.label);
    },
    m_toolsContainer);

  m_toolsLayout->addWidget(panel);
  appendLog(trText("log.toolPanel") + ": " + tool.label);
}

void MainWindow::startCameraSimulation(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    selectCamera(camera);
  }

  if (camera.type != "file")
  {
    appendLog(trText("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  QString error;
  const QString testFolder = cameraTestImagesFolder(camera);
  if (!runtime.start(camera, testFolder, &error))
  {
    appendLog(error.isEmpty() ? trText("log.cameraStartFailed") + ": " + camera.id : error);
    showCameraSetupPanel(camera);
    return;
  }

  m_simulationTimer->start(runtime.intervalMs());
  appendLog(trText("log.cameraStarted") + ": " + camera.id);
  advanceCameraFrame(camera);
  showCameraSetupPanel(camera);
}


void MainWindow::stopCameraSimulation(const CameraConfig& camera)
{
  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  runtime.stop();

  if (camera.id == m_selectedCameraId)
  {
    m_simulationTimer->stop();
    showCameraSetupPanel(camera);
  }

  appendLog(trText("log.cameraStopped") + ": " + camera.id);
}


void MainWindow::stepCameraSimulation(const CameraConfig& camera)
{
  if (camera.type != "file")
  {
    appendLog(trText("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  advanceCameraFrame(camera);
  showCameraSetupPanel(camera);
}


void MainWindow::advanceCameraFrame(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  CameraRuntime& runtime = m_cameraRuntime[camera.id];
  QString error;
  const QString testFolder = cameraTestImagesFolder(camera);
  if (!runtime.step(camera, testFolder, &error))
  {
    m_simulationTimer->stop();
    appendLog(error.isEmpty() ? trText("log.cameraNoMoreFrames") + ": " + camera.id : error);
    return;
  }

  m_selectedPreview = matToPixmap(runtime.currentFrame());
  for (CameraTileWidget* tile : m_tiles)
  {
    if (tile->camera().id == camera.id)
    {
      tile->setPreview(m_selectedPreview);
      break;
    }
  }
  m_largeImage->setImage(m_selectedPreview);
  QElapsedTimer scanTimer;
  scanTimer.start();
  processCurrentCameraFrame(camera);
  m_lastSetupScanElapsedMs[camera.id] = scanTimer.elapsed();
  updateCameraSetupDetails(camera);
}


void MainWindow::processCurrentCameraFrame(const CameraConfig& camera)
{
  auto runConfiguredGeometry = [this, camera]() {
    testConfiguredGeometryLines(camera);
  };

  if (isBwDimensionalCamera(camera))
  {
    testLocalization(camera);
    runConfiguredGeometry();
    return;
  }

  if (!isGrayscaleLocalizationCamera(camera))
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
    runConfiguredGeometry();
    return;
  }

  QRect roi;
  if (m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    testSurfaceEdgePcaLocalization(camera);
    runConfiguredGeometry();
  }
}


void MainWindow::refreshPoseForCurrentFrame(const CameraConfig& camera)
{
  if (camera.id != m_selectedCameraId)
  {
    return;
  }

  if (isBwDimensionalCamera(camera))
  {
    testLocalization(camera);
    return;
  }

  if (!isGrayscaleLocalizationCamera(camera))
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(camera.id);
  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    testSurfaceAnnulusLocalization(camera);
    return;
  }

  QRect roi;
  if (m_recipeManager.loadSurfaceDefectRoi(camera.id, roi))
  {
    testSurfaceEdgePcaLocalization(camera);
  }
}


void MainWindow::updateCameraSetupDetails(const CameraConfig& camera)
{
  if (!m_setupPanel || m_setupCameraId != camera.id)
  {
    return;
  }

  m_setupPanel->setDetailsText(cameraSetupDetailsText(camera));
}


QString MainWindow::cameraSetupDetailsText(const CameraConfig& camera) const
{
  const auto runtimeIt = m_cameraRuntime.find(camera.id);
  const CameraRuntime* runtime = runtimeIt == m_cameraRuntime.end() ? nullptr : &runtimeIt->second;
  const PartPose pose = runtime ? runtime->currentPose() : makeInvalidPartPose(camera.id);
  const QString samplePath = m_recipeManager.firstCameraSampleImagePath(camera.id);
  const QString testPath = m_recipeManager.firstCameraTestImagePath(camera.id);
  const qint64 scanElapsedMs = m_lastSetupScanElapsedMs.value(camera.id, -1);

  return QString("%1: %2\n%3: %4\n%5: %6\n%7: %8\n%9: %10\n%11: %12\n%13: %14\n%15: %16")
    .arg(trText("labels.source"), camera.type)
    .arg(trText("labels.folder"), resolvedCameraFolder(camera))
    .arg(trText("labels.sampleImage"), samplePath.isEmpty() ? trText("status.invalid") : samplePath)
    .arg(trText("labels.testImages"), testPath.isEmpty() ? trText("status.invalid") : m_recipeManager.cameraTestImagesPath(camera.id))
    .arg(trText("labels.status"), runtime ? runtimeStatusText(runtime->status()) : trText("status.stopped"))
    .arg(trText("labels.frame"), runtime ? QString::number(runtime->frameIndex()) : QStringLiteral("0"))
    .arg(
      trText("labels.partPose"),
      pose.valid
        ? QString("X=%1 Y=%2 A=%3 S=%4")
            .arg(pose.origin.x, 0, 'f', 1)
            .arg(pose.origin.y, 0, 'f', 1)
            .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
            .arg(pose.score, 0, 'f', 2)
        : trText("status.invalid"))
    .arg(trText("labels.scanTime"), scanElapsedMs >= 0 ? QString("%1 ms").arg(scanElapsedMs) : trText("status.invalid"));
}

