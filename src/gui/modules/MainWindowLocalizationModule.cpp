#include "gui/modules/MainWindowLocalizationModule.h"

#include "gui/SurfaceLocalizationAdapters.h"
#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "util/AsyncExecutor.h"

#include <memory>

using AsyncExecutor::runAsyncTask;
using namespace SurfaceLocalizationAdapters;

MainWindowLocalizationModule::MainWindowLocalizationModule(MainWindowContext& context)
  : MainWindowModuleBase(context)
{
}

void MainWindowLocalizationModule::testLocalization(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isBwDimensional(camera, config()))
  {
    log(tr("log.localizationNotAvailable") + ": " + camera.id);
    return;
  }

  QRect roi;
  if (!recipes().loadLocalizationRoi(camera.id, roi))
  {
    log(tr("log.localizationRoiMissing") + ": " + camera.id);
    return;
  }

  MainWindowImagingModule& imaging = *context().imaging;
  QString imageError;
  const cv::Mat input = imaging.currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const LocalizationSettings settings = recipes().loadLocalizationSettings(camera.id);
  const QVector<QRect> exclusionRects = recipes().loadLocalizationExclusionRects(camera.id);

  auto job = [input, roi, exclusionRects, settings]() -> LocalizationResult {
    LocalizationProcessor processor;
    return processor.locateDarkObjectOnLightBackground(
      input,
      cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
      toCvRects(exclusionRects),
      settings.thresholdFactor,
      settings.thresholdOffset);
  };

  const QString pendingCameraId = camera.id;
  const int setupFrameIndex = cameraRuntime()[camera.id].frameIndex();
  if (context().incPendingJobs)
  {
    context().incPendingJobs(pendingCameraId);
  }

  runAsyncTask(
    decltype(job)(job),
    window(),
    [this, camera, roi, settings, pendingCameraId, setupFrameIndex](const LocalizationResult& result) {
      if (context().decPendingJobs)
      {
        context().decPendingJobs(pendingCameraId);
      }
      if (*context().setupCameraId == camera.id && cameraRuntime()[camera.id].frameIndex() != setupFrameIndex)
      {
        return;
      }

      const bool updateView =
        camera.id == selectedCameraId() &&
        *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry &&
        *context().setupCameraId != camera.id;

      if (result.diagnosticImage.empty())
      {
        context().lastLocalizationResults->remove(camera.id);
        cameraRuntime()[camera.id].clearCurrentPose(camera.id);
        if (updateView)
        {
          largeImage()->clearGeometryOverlay();
        }
        log(tr("log.localizationFailed") + ": " + camera.id);
        return;
      }

      if (updateView)
      {
        selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
        largeImage()->setImage(selectedPreview());
        largeImage()->setRoi(roi);
      }
      context().lastLocalizationResults->insert(camera.id, result);

      if (!result.found)
      {
        cameraRuntime()[camera.id].clearCurrentPose(camera.id);
        if (updateView)
        {
          largeImage()->clearGeometryOverlay();
        }
        log(tr("log.localizationNotFound") + ": " + camera.id);
        return;
      }

      cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromLocalizationResult(camera, result));
      if (context().setup)
      {
        context().setup->refreshSetupGeometryResults(camera);
        return;
      }

      GeometryOverlay overlay;
      if (context().geometry)
      {
        context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
      }
      if (updateView)
      {
        largeImage()->setGeometryOverlay(overlay);
      }
      log(QString("%1: %2 cx=%3 cy=%4 angle=%5 area=%6 thr=%7 factor=%8 offset=%9")
            .arg(tr("log.localizationFound"))
            .arg(camera.id)
            .arg(result.center.x, 0, 'f', 1)
            .arg(result.center.y, 0, 'f', 1)
            .arg(result.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
            .arg(result.area, 0, 'f', 1)
            .arg(result.thresholdValue, 0, 'f', 1)
            .arg(settings.thresholdFactor, 0, 'f', 3)
            .arg(settings.thresholdOffset, 0, 'f', 1));
    });
}
