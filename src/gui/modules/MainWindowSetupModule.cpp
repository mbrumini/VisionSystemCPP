#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowLocalizationModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/setup/SetupResultsDialog.h"
#include "gui/modules/setup/SetupResultsFormatter.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QPushButton>

#include <opencv2/core.hpp>

MainWindowSetupModule::MainWindowSetupModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

QString MainWindowSetupModule::runtimeStatusText(const MainWindowContext& context, CameraRuntime::Status status)
{
  switch (status)
  {
  case CameraRuntime::Status::Ready:
    return context.trText("status.ready");
  case CameraRuntime::Status::Running:
    return context.trText("status.running");
  case CameraRuntime::Status::Error:
    return context.trText("status.error");
  case CameraRuntime::Status::Stopped:
  default:
    return context.trText("status.stopped");
  }
}

void MainWindowSetupModule::showCameraSetupPanel(const CameraConfig& camera)
{
  context().deactivateImageDrawingTools();
  context().clearToolPanel();

  CameraRuntime& runtime = cameraRuntime()[camera.id];
  if (!runtime.running())
  {
    context().geometry->restoreCleanGeometryImage(camera);
  }
  CameraSetupPanelTexts texts;
  texts.title = QString("%1 | %2").arg(tr("tools.setup"), camera.id);
  texts.details = cameraSetupDetailsText(camera);
  texts.frameInterval = tr("labels.frameInterval");
  texts.acquireSample = tr("actions.acquireSampleImage");
  texts.importSample = tr("actions.assignSampleImage");
  texts.showSample = tr("labels.sampleImage");
  texts.importTests = tr("actions.assignTestImages");
  texts.start = tr("commands.start");
  texts.stop = tr("commands.stop");
  texts.nextFrame = tr("actions.nextFrame");
  texts.results = tr("actions.frameResults");
  texts.back = tr("commands.backToCameraTools");
  texts.toolsTitle = tr("tools.geometries");
  texts.toolLabels = {
    tr("actions.pointGeometry"),
    tr("actions.lineGeometry"),
    tr("actions.circleGeometry"),
    tr("actions.arcGeometry")
  };

  auto* panel = new CameraSetupPanelWidget(
    texts,
    runtime.intervalMs(),
    [this, camera](int value) {
      CameraRuntime& runtime = cameraRuntime()[camera.id];
      runtime.setIntervalMs(value);
      if (runtime.running() && camera.id == selectedCamera().id)
      {
        context().simulationTimer->start(runtime.intervalMs());
      }
    },
    [this, camera]() { context().cameraConfig->acquireCameraSampleImage(camera); },
    [this, camera]() { context().cameraConfig->configureCameraSampleImage(camera); },
    [this, camera]() { showCameraSampleImage(camera); },
    [this, camera]() { context().cameraConfig->configureCameraTestImages(camera); },
    [this, camera]() { startCameraSimulation(camera); },
    [this, camera]() { stopCameraSimulation(camera); },
    [this, camera]() { stepCameraSimulation(camera); },
    [this, camera]() { showSetupResultsPopup(camera); },
    {
      [this, camera]() {
        *context().returnToSetupCameraId = camera.id;
        context().geometry->showGeometryPointPanel(camera);
      },
      [this, camera]() {
        *context().returnToSetupCameraId = camera.id;
        context().geometry->showGeometryLinePanel(camera);
      },
      [this, camera]() {
        *context().returnToSetupCameraId = camera.id;
        context().geometry->showGeometryCirclePanel(camera);
      },
      [this, camera]() {
        *context().returnToSetupCameraId = camera.id;
        context().geometry->showGeometryArcPanel(camera);
      }
    },
    [this, camera]() {
      stopCameraSimulation(camera);
      context().returnToSetupCameraId->clear();
      context().imaging->reloadCameraReferenceImage(camera);
      context().showCameraToolList(camera);
    },
    toolsContainer());
  *context().setupPanel = panel;
  panel->setDetailsVisible(context().setupDetailsVisible && *context().setupDetailsVisible);
  *context().setupCameraId = camera.id;
  toolsLayout()->addWidget(panel);
  refreshSetupGeometryResults(camera);
}

void MainWindowSetupModule::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
  context().deactivateImageDrawingTools();

  if (toolId == "geometries")
  {
    context().geometry->showGeometryPanel(camera);
    return;
  }

  if (toolId == "surfaceLocalization")
  {
    context().surface->showSurfaceLocalizationPanel(camera);
    return;
  }

  if (toolId == "measurements")
  {
    context().measurement->showMeasurementPanel(camera);
    return;
  }

  context().clearToolPanel();

  const ToolDefinition tool = ToolCatalog::tool(toolId, translations());
  const QString noteText = tool.id == "surfaceLocalization"
    ? tr("labels.surfaceLocalizationNote")
    : tr("labels.placeholderNote");
  auto* panel = new ToolPanelWidget(
    tool,
    QString("%1 %2").arg(tr("labels.camera")).arg(camera.slot),
    noteText,
    tr("commands.backToCameraTools"),
    [this, camera]() {
      context().showCameraToolList(camera);
      log(tr("log.backToCameraTools") + ": " + camera.id);
    },
    [this, camera, tool](const ToolActionDefinition& action) {
      if (tool.id == "localization" && action.id == "searchRoi")
      {
        context().surface->activateLocalizationRoiDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "addExclusion")
      {
        context().surface->activateLocalizationExclusionDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "clearExclusions")
      {
        context().surface->clearLocalizationExclusions(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "testLocalization")
      {
        context().localization->testLocalization(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "pointGeometry")
      {
        context().geometry->showGeometryPointPanel(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "lineGeometry")
      {
        context().geometry->showGeometryLinePanel(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "circleGeometry")
      {
        context().geometry->showGeometryCirclePanel(camera);
        return;
      }

      if (tool.id == "geometries" && action.id == "arcGeometry")
      {
        context().geometry->showGeometryArcPanel(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceSearchRoi")
      {
        context().surface->activateSurfaceDefectRoiDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceOuterCircle")
      {
        context().surface->activateSurfaceOuterCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceInnerCircle")
      {
        context().surface->activateSurfaceInnerCircleDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceThreshold")
      {
        context().surface->saveSurfaceThresholdAndPreview(camera, recipes().loadSurfaceAnnulusLocalization(camera.id).thresholdMax);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceAnnulus")
      {
        context().surface->testSurfaceAnnulusLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceAddExclusion")
      {
        context().surface->activateSurfaceDefectExclusionDrawing(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "surfaceClearExclusions")
      {
        context().surface->clearSurfaceDefectExclusions(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceLocalization")
      {
        context().surface->testSurfaceLocalization(camera);
        return;
      }

      if (tool.id == "surfaceLocalization" && action.id == "testSurfaceStrategy")
      {
        context().surface->testSurfaceLocalizationStrategy(camera);
        return;
      }

      log(tr("log.placeholder") + ": " + tool.label + " -> " + action.label);
    },
    toolsContainer());

  toolsLayout()->addWidget(panel);
  log(tr("log.toolPanel") + ": " + tool.label);
}

void MainWindowSetupModule::startCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel)
{
  if (camera.id != selectedCamera().id)
  {
    context().selectCamera(camera);
  }

  if (camera.type != "file" && camera.type != "usb")
  {
    log(tr("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[camera.id];
  QString error;
  const QString testFolder = context().imaging->cameraTestImagesFolder(camera);
  if (!runtime.start(camera, testFolder, &error))
  {
    log(error.isEmpty() ? tr("log.cameraStartFailed") + ": " + camera.id : error);
    if (refreshSetupPanel)
    {
      showCameraSetupPanel(camera);
    }
    return;
  }

  context().simulationTimer->start(runtime.intervalMs());
  log(tr("log.cameraStarted") + ": " + camera.id);
  advanceCameraFrame(camera);
  if (refreshSetupPanel)
  {
    showCameraSetupPanel(camera);
  }
}


void MainWindowSetupModule::stopCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel)
{
  CameraRuntime& runtime = cameraRuntime()[camera.id];
  runtime.stop();

  if (camera.id == selectedCamera().id)
  {
    context().simulationTimer->stop();
    if (refreshSetupPanel)
    {
      showCameraSetupPanel(camera);
    }
  }

  log(tr("log.cameraStopped") + ": " + camera.id);
}

void MainWindowSetupModule::showCameraSampleImage(const CameraConfig& camera)
{
  if (camera.id != selectedCamera().id)
  {
    context().selectCamera(camera);
  }

  CameraRuntime& runtime = cameraRuntime()[camera.id];
  runtime.stop();
  if (camera.id == selectedCamera().id)
  {
    context().simulationTimer->stop();
  }

  context().imaging->reloadCameraReferenceImage(camera);
  showCameraSetupPanel(camera);
  log(tr("labels.sampleImage") + ": " + camera.id);
}


void MainWindowSetupModule::stepCameraSimulation(const CameraConfig& camera)
{
  if (camera.type != "file" && camera.type != "usb")
  {
    log(tr("log.cameraSourceUnsupported") + ": " + camera.id);
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[camera.id];
  advanceCameraFrame(camera);
  showCameraSetupPanel(camera);
}


void MainWindowSetupModule::advanceCameraFrame(const CameraConfig& camera)
{
  if (camera.id != selectedCamera().id)
  {
    return;
  }

  if (context().cameraPendingJobs && context().cameraPendingJobs->value(camera.id, 0) > 0)
  {
    (*context().cameraDroppedFrames)[camera.id] = context().cameraDroppedFrames->value(camera.id, 0) + 1;
    updateCameraSetupDetails(camera);
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[camera.id];
  QString error;
  const QString testFolder = context().imaging->cameraTestImagesFolder(camera);
  if (!runtime.step(camera, testFolder, &error))
  {
    context().simulationTimer->stop();
    log(error.isEmpty() ? tr("log.cameraNoMoreFrames") + ": " + camera.id : error);
    return;
  }

  log(QString("pipeline frame begin: %1 frame=%2").arg(camera.id).arg(runtime.frameIndex()));
  selectedPreview() = context().imaging->matToPixmap(runtime.currentFrame());
  for (CameraTileWidget* tile : *context().tiles)
  {
    if (tile->camera().id == camera.id)
    {
      tile->setPreview(selectedPreview());
      break;
    }
  }
  largeImage()->setImage(selectedPreview());
  if (context().geometry)
  {
    context().geometry->showRuntimeGeometryOverlay(camera);
  }
  QElapsedTimer scanTimer;
  scanTimer.start();
  processCurrentCameraFrame(camera);
  (*context().lastSetupScanElapsedMs)[camera.id] = scanTimer.elapsed();
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
  log(QString("pipeline frame end: %1 frame=%2 elapsedMs=%3 pendingJobs=%4")
        .arg(camera.id)
        .arg(runtime.frameIndex())
        .arg((*context().lastSetupScanElapsedMs)[camera.id])
        .arg(context().cameraPendingJobs->value(camera.id, 0)));
  updateCameraSetupDetails(camera);
}


void MainWindowSetupModule::processCurrentCameraFrame(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(QString("pipeline localization skipped: %1 profile=%2").arg(camera.id, camera.profile.id));
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  QRect roi;
  if (annulus.method == "massPca")
  {
    if (recipes().loadSurfaceDefectRoi(camera.id, roi))
    {
      log(QString("pipeline surface mass pca begin: %1 roi=%2,%3 %4x%5")
            .arg(camera.id)
            .arg(roi.x())
            .arg(roi.y())
            .arg(roi.width())
            .arg(roi.height()));
      context().surface->testSurfaceLocalization(camera);
      return;
    }
    log(QString("pipeline surface mass pca missing roi: %1").arg(camera.id));
    return;
  }

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method == "threshold" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    log(QString("pipeline surface localization begin: %1 method=%2").arg(camera.id, annulus.method));
    context().surface->testSurfaceAnnulusLocalization(camera);
    return;
  }

  if (recipes().loadSurfaceDefectRoi(camera.id, roi))
  {
    log(QString("pipeline surface pca begin: %1 roi=%2,%3 %4x%5")
          .arg(camera.id)
          .arg(roi.x())
          .arg(roi.y())
          .arg(roi.width())
          .arg(roi.height()));
    context().surface->testSurfaceEdgePcaLocalization(camera);
    return;
  }

  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    log(QString("pipeline localization begin: %1 mode=bw").arg(camera.id));
    context().localization->testLocalization(camera);
    return;
  }

  log(QString("pipeline localization missing setup: %1").arg(camera.id));
}

void MainWindowSetupModule::refreshSetupGeometryResults(const CameraConfig& camera)
{
  if (camera.id != selectedCamera().id || !context().geometry)
  {
    return;
  }

  context().geometry->testConfiguredGeometryLines(camera);
  if (context().constructedGeometry)
  {
    context().constructedGeometry->rebuildConstructedGeometryRecipe(camera);
  }
  if (context().measurement)
  {
    context().measurement->rebuildMeasurementRecipe(camera);
    GeometryOverlay overlay = largeImage()->geometryOverlay();
    context().measurement->appendMeasurementOverlay(camera, overlay);
    largeImage()->setGeometryOverlay(overlay);
  }
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}


void MainWindowSetupModule::refreshPoseForCurrentFrame(const CameraConfig& camera)
{
  if (camera.id != selectedCamera().id)
  {
    return;
  }

  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  QRect roi;
  if (annulus.method == "massPca")
  {
    if (recipes().loadSurfaceDefectRoi(camera.id, roi))
    {
      context().surface->testSurfaceLocalization(camera);
    }
    return;
  }

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method == "threshold" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    context().surface->testSurfaceAnnulusLocalization(camera);
    return;
  }

  if (recipes().loadSurfaceDefectRoi(camera.id, roi))
  {
    context().surface->testSurfaceEdgePcaLocalization(camera);
    return;
  }

  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    context().localization->testLocalization(camera);
  }
}


void MainWindowSetupModule::updateCameraSetupDetails(const CameraConfig& camera)
{
  if (!context().setupPanel || !*context().setupPanel || *context().setupCameraId != camera.id)
  {
    return;
  }

  (*context().setupPanel)->setDetailsText(cameraSetupDetailsText(camera));
  updateSetupResultsPopup(camera);
}


QString MainWindowSetupModule::cameraSetupDetailsText(const CameraConfig& camera) const
{
  const auto runtimeIt = cameraRuntime().find(camera.id);
  const CameraRuntime* runtime = runtimeIt == cameraRuntime().end() ? nullptr : &runtimeIt->second;
  const PartPose pose = runtime ? runtime->currentPose() : makeInvalidPartPose(camera.id);
  const QString samplePath = recipes().firstCameraSampleImagePath(camera.id);
  const QString testPath = recipes().firstCameraTestImagePath(camera.id);
  const qint64 scanElapsedMs = context().lastSetupScanElapsedMs->value(camera.id, -1);
  QString sourceLabel = tr("labels.folder");
  QString sourceDetails = context().imaging->resolvedCameraFolder(camera);
  if (camera.type == "vimba")
  {
    sourceLabel = "VimbaX";
    sourceDetails = QString("serial=%1 id=%2 interface=%3 trigger=%4/%5")
      .arg(camera.serial.isEmpty() ? tr("status.invalid") : camera.serial)
      .arg(camera.deviceId.isEmpty() ? tr("status.invalid") : camera.deviceId)
      .arg(camera.interfaceId.isEmpty() ? tr("status.invalid") : camera.interfaceId)
      .arg(camera.trigger.mode.isEmpty() ? tr("status.invalid") : camera.trigger.mode)
      .arg(camera.trigger.source.isEmpty() ? tr("status.invalid") : camera.trigger.source);
  }
  else if (camera.type == "usb")
  {
    sourceLabel = "USB";
    sourceDetails = QString("index=%1 id=%2 trigger=%3/%4")
      .arg(camera.usbIndex)
      .arg(camera.deviceId.isEmpty() ? tr("status.invalid") : camera.deviceId)
      .arg(camera.trigger.mode.isEmpty() ? tr("status.invalid") : camera.trigger.mode)
      .arg(camera.trigger.source.isEmpty() ? tr("status.invalid") : camera.trigger.source);
  }

  return QString("%1: %2\n%3: %4\n%5: %6\n%7: %8\n%9: %10\n%11: %12\n%13: %14\n%15: %16")
    .arg(tr("labels.source"), camera.type)
    .arg(sourceLabel, sourceDetails)
    .arg(tr("labels.sampleImage"), samplePath.isEmpty() ? tr("status.invalid") : samplePath)
    .arg(tr("labels.testImages"), testPath.isEmpty() ? tr("status.invalid") : recipes().cameraTestImagesPath(camera.id))
    .arg(tr("labels.status"), runtime ? runtimeStatusText(context(), runtime->status()) : tr("status.stopped"))
    .arg(tr("labels.frame"), runtime ? QString::number(runtime->frameIndex()) : QStringLiteral("0"))
    .arg(
      tr("labels.partPose"),
      pose.valid
        ? QString("X=%1 Y=%2 A=%3 S=%4")
            .arg(pose.origin.x, 0, 'f', 1)
            .arg(pose.origin.y, 0, 'f', 1)
            .arg(pose.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
            .arg(pose.score, 0, 'f', 2)
        : tr("status.invalid"))
    .arg(tr("labels.scanTime"), scanElapsedMs >= 0 ? QString("%1 ms").arg(scanElapsedMs) : tr("status.invalid"));
}

void MainWindowSetupModule::showSetupResultsPopup(const CameraConfig& camera)
{
  if (!m_resultsDialog)
  {
    m_resultsDialog = new SetupResultsDialog(window());
    m_resultsDialog->setWindowTitle(tr("actions.frameResults"));
  }

  updateSetupResultsPopup(camera);
  m_resultsDialog->show();
  m_resultsDialog->raise();
  m_resultsDialog->activateWindow();
}

void MainWindowSetupModule::updateSetupResultsPopup(const CameraConfig& camera)
{
  if (!m_resultsDialog)
  {
    return;
  }

  const auto runtimeIt = cameraRuntime().find(camera.id);
  const CameraRuntime* runtime = runtimeIt == cameraRuntime().end() ? nullptr : &runtimeIt->second;
  m_resultsDialog->setResultsText(setupResultsText(camera, runtime, recipes(), context()));
}

