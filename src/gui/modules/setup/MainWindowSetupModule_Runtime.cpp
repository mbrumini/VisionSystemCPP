#include "gui/modules/MainWindowSetupModule.h"

#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowConstructedGeometryModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowLocalizationModule.h"
#include "gui/modules/MainWindowMeasurementModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraConfigModule.h"
#include "gui/modules/setup/SetupCameraResolver.h"
#include "util/AsyncExecutor.h"

#include <QElapsedTimer>
#include <QTimer>

#include <opencv2/core.hpp>

void MainWindowSetupModule::startCameraSimulation(const CameraConfig& camera, bool refreshSetupPanel)
{
  const CameraConfig effectiveCamera = currentConfiguredCamera(config(), camera);

  if (effectiveCamera.type == "usb" && effectiveCamera.usbIndex < 0)
  {
    if (context().cameraConfig)
    {
      context().cameraConfig->configureUsbCameraSlot(effectiveCamera.slot, effectiveCamera.id);
    }
    return;
  }

  if (effectiveCamera.type != "file" && effectiveCamera.type != "usb" &&
      effectiveCamera.type != "vimba" && effectiveCamera.type != "simulator")
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

  if (effectiveCamera.type == "simulator")
  {
    context().simulationTimer->stop();
    log(QString("Simulatore armato: %1 channel=%2")
      .arg(
        effectiveCamera.id,
        effectiveCamera.simulatorChannel.isEmpty()
          ? effectiveCamera.id
          : effectiveCamera.simulatorChannel));
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

  if (effectiveCamera.type == "usb" && effectiveCamera.usbIndex < 0)
  {
    if (context().cameraConfig)
    {
      context().cameraConfig->configureUsbCameraSlot(effectiveCamera.slot, effectiveCamera.id);
    }
    return;
  }

  if (effectiveCamera.type != "file" && effectiveCamera.type != "usb" &&
      effectiveCamera.type != "vimba" && effectiveCamera.type != "simulator")
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

  if (m_cameraGrabInProgress.value(effectiveCamera.id, false))
  {
    return;
  }

  if (context().cameraPendingJobs && context().cameraPendingJobs->value(effectiveCamera.id, 0) > 0)
  {
    (*context().cameraDroppedFrames)[effectiveCamera.id] = context().cameraDroppedFrames->value(effectiveCamera.id, 0) + 1;
    updateCameraSetupDetails(effectiveCamera);
    return;
  }

  m_cameraGrabInProgress[effectiveCamera.id] = true;
  CameraRuntime* runtimePtr = &cameraRuntime()[effectiveCamera.id];
  const QString testFolder = context().imaging->cameraTestImagesFolder(effectiveCamera);

  auto grabJob = [runtimePtr, effectiveCamera, testFolder]() {
    struct GrabTaskResult {
      bool success = false;
      cv::Mat frame;
      SimulatorFrameMetadata metadata;
      QString error;
    } result;
    result.success = runtimePtr->grabFrame(effectiveCamera, testFolder, result.frame, result.metadata, result.error);
    return result;
  };

  AsyncExecutor::runAsyncTask(
    std::move(grabJob),
    window(),
    [this, runtimePtr, effectiveCamera](auto grabResult) {
      m_cameraGrabInProgress[effectiveCamera.id] = false;

      if (!runtimePtr->running())
      {
        return;
      }

      if (!grabResult.success)
      {
        if (effectiveCamera.type == "simulator" && runtimePtr->running())
        {
          updateCameraSetupDetails(effectiveCamera);
          return;
        }
        log(grabResult.error.isEmpty() ? tr("log.cameraNoMoreFrames") + ": " + effectiveCamera.id : grabResult.error);
        if (effectiveCamera.type == "vimba" && runtimePtr->running())
        {
          updateCameraSetupDetails(effectiveCamera);
          return;
        }
        runtimePtr->stop();
        return;
      }

      runtimePtr->updateStateAfterGrab(grabResult.frame, grabResult.metadata);

      log(QString("pipeline frame begin: %1 frame=%2").arg(effectiveCamera.id).arg(runtimePtr->frameIndex()));
      const QPixmap framePreview =
        context().imaging->matToPixmap(runtimePtr->currentFrame(), QSize(1920, 1080));
      for (CameraTileWidget* tile : *context().tiles)
      {
        if (tile->camera().id == effectiveCamera.id)
        {
          tile->setPreview(framePreview);
          break;
        }
      }
      if (effectiveCamera.id == selectedCamera().id)
      {
        selectedPreview() = framePreview;
        largeImage()->setImage(selectedPreview());
      }
      QElapsedTimer scanTimer;
      scanTimer.start();
      processCurrentCameraFrame(effectiveCamera);
      (*context().lastSetupScanElapsedMs)[effectiveCamera.id] = scanTimer.elapsed();
      if (context().publishSimulatorResult)
      {
        QTimer::singleShot(0, window(), [this, cameraId = effectiveCamera.id]() {
          if (context().cameraPendingJobs->value(cameraId, 0) == 0)
          {
            context().publishSimulatorResult(cameraId);
          }
        });
      }
      if (effectiveCamera.id == selectedCamera().id && context().updateMeasurementResults)
      {
        context().updateMeasurementResults();
      }
      if (context().machineRunning && *context().machineRunning &&
          context().cameraPendingJobs->value(effectiveCamera.id, 0) == 0 &&
          context().notifyProductionPieceCompleted)
      {
        context().notifyProductionPieceCompleted(effectiveCamera.id);
      }
      log(QString("pipeline frame end: %1 frame=%2 elapsedMs=%3 pendingJobs=%4")
            .arg(effectiveCamera.id)
            .arg(runtimePtr->frameIndex())
            .arg((*context().lastSetupScanElapsedMs)[effectiveCamera.id])
            .arg(context().cameraPendingJobs->value(effectiveCamera.id, 0)));
      updateCameraSetupDetails(effectiveCamera);
    },
    QString("grab.%1").arg(effectiveCamera.id));
}


void MainWindowSetupModule::processCurrentCameraFrame(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(QString("pipeline localization skipped: %1 profile=%2").arg(camera.id, camera.profile.id));
    refreshSetupGeometryResults(camera);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  QRect roi;
  if (annulus.method == "two_circles_axis")
  {
    log(QString("pipeline surface strategy begin: %1").arg(camera.id));
    context().surface->testSurfaceLocalizationStrategy(camera);
    return;
  }

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
  if (!context().geometry)
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
    if (camera.id == selectedCamera().id)
    {
      GeometryOverlay overlay = largeImage()->geometryOverlay();
      context().measurement->appendMeasurementOverlay(camera, overlay);
      largeImage()->setGeometryOverlay(overlay);
    }
  }
  if (camera.id == selectedCamera().id && context().updateMeasurementResults)
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
  if (annulus.method == "two_circles_axis")
  {
    context().surface->testSurfaceLocalizationStrategy(camera);
    return;
  }

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


