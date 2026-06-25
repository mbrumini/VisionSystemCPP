#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowLocalizationModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowThreadModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/setup/SetupCameraResolver.h"
#include "gui/modules/setup/SetupResultsDialog.h"
#include "gui/modules/setup/SetupResultsFormatter.h"
#include "gui/ToolCatalog.h"
#include "gui/ToolPanelWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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
  texts.acquireAiSample = tr("tools.ai");
  texts.importSample = tr("actions.assignSampleImage");
  texts.showSample = tr("labels.sampleImage");
  texts.importTests = tr("actions.assignTestImages");
  texts.acquisitionSettings = tr("actions.cameraAcquisitionSettings");
  const bool grabRunning = cameraRuntime()[camera.id].running();
  texts.grabToggle = grabRunning ? tr("commands.stop") : tr("commands.start");
  texts.grabToggleIcon = grabRunning ? "stop" : "start";
  texts.nextFrame = tr("actions.nextFrame");
  texts.results = tr("actions.frameResults");
  texts.back = tr("commands.backToCameraTools");
  texts.recipeImagesTitle = tr("groups.recipeImages");
  texts.cameraSetupTitle = tr("groups.cameraSetup");
  texts.aoi = tr("actions.defineAoi");

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
    [this, camera]() { showCameraAcquisitionPanel(camera); },
    [this, camera]() {
      if (cameraRuntime()[camera.id].running())
      {
        stopCameraSimulation(camera);
        return;
      }
      startCameraSimulation(camera);
    },
    [this, camera]() { stepCameraSimulation(camera); },
    [this, camera]() { showSetupResultsPopup(camera); },
    [this, camera]() { activateGlobalAoiDrawing(camera); },
    {},
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

  QRect aoi;
  if (recipes().loadGlobalSurfaceAoi(camera.id, aoi))
  {
    largeImage()->setRoi(aoi);
  }
  else
  {
    largeImage()->clearRoi();
  }

  refreshSetupGeometryResults(camera);
}

void MainWindowSetupModule::showToolPanel(const CameraConfig& camera, const QString& toolId)
{
  context().deactivateImageDrawingTools();
  if (camera.id == selectedCameraId() &&
      !cameraRuntime()[camera.id].running() &&
      cameraRuntime()[camera.id].currentFrame().empty())
  {
    context().imaging->ensureReferenceImageVisible(camera);
  }

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

  if (toolId == "ai")
  {
    showAiPanel(camera);
    return;
  }

  if (toolId == "aiLocalization")
  {
    if (context().optionalToolVisible && !context().optionalToolVisible(toolId))
    {
      context().showCameraToolList(camera);
      return;
    }
    showAiLocalizationPanel(camera);
    return;
  }

  if (toolId == "aiClassification")
  {
    if (context().optionalToolVisible && !context().optionalToolVisible(toolId))
    {
      context().showCameraToolList(camera);
      return;
    }
    showAiClassificationPanel(camera);
    return;
  }

  if (toolId == "threadInspection")
  {
    context().thread->showThreadInspectionPanel(camera);
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
        context().imaging->ensureReferenceImageVisible(camera);
        context().surface->activateLocalizationRoiDrawing(camera);
        return;
      }

      if (tool.id == "localization" && action.id == "addExclusion")
      {
        context().imaging->ensureReferenceImageVisible(camera);
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
        context().imaging->ensureReferenceImageVisible(camera);
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

void MainWindowSetupModule::activateGlobalAoiDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().imaging->ensureReferenceImageVisible(camera);
  largeImage()->setRoiDrawingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::GlobalAoi;
  log(tr("actions.defineAoi") + ": " + camera.id);
}

