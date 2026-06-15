#include "gui/modules/MainWindowSetupModule.h"

#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowLocalizationModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/setup/SetupCameraResolver.h"

#include <QElapsedTimer>
#include <QTimer>

#include <opencv2/core.hpp>

void MainWindowSetupModule::startCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);

  if (effectiveCamera.id != selectedCamera().id ||
      effectiveCamera.type != selectedCamera().type ||
      effectiveCamera.usbIndex != selectedCamera().usbIndex ||
      effectiveCamera.deviceId != selectedCamera().deviceId)
  {
    context().selectCamera(effectiveCamera);
  }

  if (effectiveCamera.type != "file" && effectiveCamera.type != "usb")
  {
    log(tr("log.cameraSourceUnsupported") + ": " + effectiveCamera.id);
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[effectiveCamera.id];
  QString error;
  const QString testFolder = context().imaging->cameraTestImagesFolder(effectiveCamera);
  if (!runtime.start(effectiveCamera, testFolder, &error))
  {
    log(error.isEmpty() ? tr("log.cameraStartFailed") + ": " + effectiveCamera.id : error);
    if (refreshSetupPanel)
    {
      showCameraSetupPanel(effectiveCamera);
    }
    return;
  }

  context().simulationTimer->start(runtime.intervalMs());
  log(tr("log.cameraStarted") + ": " + effectiveCamera.id);
  advanceCameraFrame(effectiveCamera);
  if (refreshSetupPanel)
  {
    showCameraSetupPanel(effectiveCamera);
  }
}


void MainWindowSetupModule::stopCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel)
{
  if (m_aiClassificationCaptureCameraId == camera.id)
  {
    stopAiClassificationCapture();
  }

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
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);

  if (effectiveCamera.type != "file" && effectiveCamera.type != "usb")
  {
    log(tr("log.cameraSourceUnsupported") + ": " + effectiveCamera.id);
    return;
  }

  advanceCameraFrame(effectiveCamera);
  showCameraSetupPanel(effectiveCamera);
}


void MainWindowSetupModule::advanceCameraFrame(const CameraConfig& camera)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);

  if (effectiveCamera.id != selectedCamera().id)
  {
    return;
  }

  if (context().cameraPendingJobs && context().cameraPendingJobs->value(effectiveCamera.id, 0) > 0)
  {
    (*context().cameraDroppedFrames)[effectiveCamera.id] = context().cameraDroppedFrames->value(effectiveCamera.id, 0) + 1;
    updateCameraSetupDetails(effectiveCamera);
    return;
  }

  CameraRuntime& runtime = cameraRuntime()[effectiveCamera.id];
  QString error;
  const QString testFolder = context().imaging->cameraTestImagesFolder(effectiveCamera);
  if (!runtime.step(effectiveCamera, testFolder, &error))
  {
    context().simulationTimer->stop();
    log(error.isEmpty() ? tr("log.cameraNoMoreFrames") + ": " + effectiveCamera.id : error);
    return;
  }

  log(QString("pipeline frame begin: %1 frame=%2").arg(effectiveCamera.id).arg(runtime.frameIndex()));
  selectedPreview() = context().imaging->matToPixmap(runtime.currentFrame());
  for (CameraTileWidget* tile : *context().tiles)
  {
    if (tile->camera().id == effectiveCamera.id)
    {
      tile->setPreview(selectedPreview());
      break;
    }
  }
  largeImage()->setImage(selectedPreview());
  if (context().geometry)
  {
    context().geometry->showRuntimeGeometryOverlay(effectiveCamera);
  }
  QElapsedTimer scanTimer;
  scanTimer.start();
  processCurrentCameraFrame(effectiveCamera);
  (*context().lastSetupScanElapsedMs)[effectiveCamera.id] = scanTimer.elapsed();
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
  log(QString("pipeline frame end: %1 frame=%2 elapsedMs=%3 pendingJobs=%4")
        .arg(effectiveCamera.id)
        .arg(runtime.frameIndex())
        .arg((*context().lastSetupScanElapsedMs)[effectiveCamera.id])
        .arg(context().cameraPendingJobs->value(effectiveCamera.id, 0)));
  updateCameraSetupDetails(effectiveCamera);
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

  if (annulus.method == "edgePca")
  {
    if (recipes().loadSurfaceDefectRoi(camera.id, roi) || recipes().loadSurfaceDefectPolygon(camera.id).size() >= 3)
    {
      log(QString("pipeline surface edge pca begin: %1 method=%2").arg(camera.id, annulus.method));
      context().surface->testSurfaceEdgePcaLocalization(camera);
      return;
    }
    log(QString("pipeline surface edge pca missing area: %1").arg(camera.id));
    return;
  }

  if (annulus.method == "model")
  {
    const SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
    if (model.hasModel)
    {
      log(QString("pipeline surface model begin: %1").arg(camera.id));
      context().surface->testSurfaceShapeModel(camera);
      return;
    }
    log(QString("pipeline surface model missing setup: %1").arg(camera.id));
    return;
  }

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method == "threshold" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    log(QString("pipeline surface localization begin: %1 method=%2").arg(camera.id, annulus.method));
    context().surface->testSurfaceAnnulusLocalization(camera);
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

  if (annulus.method == "edgePca")
  {
    if (recipes().loadSurfaceDefectRoi(camera.id, roi) || recipes().loadSurfaceDefectPolygon(camera.id).size() >= 3)
    {
      context().surface->testSurfaceEdgePcaLocalization(camera);
    }
    return;
  }

  if (annulus.method == "model")
  {
    if (recipes().loadSurfaceModel(camera.id).hasModel)
    {
      context().surface->testSurfaceShapeModel(camera);
    }
    return;
  }

  if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
      (annulus.method == "threshold" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
  {
    context().surface->testSurfaceAnnulusLocalization(camera);
    return;
  }

  if (MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    context().localization->testLocalization(camera);
  }
}


