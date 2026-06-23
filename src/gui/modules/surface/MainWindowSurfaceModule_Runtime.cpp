#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationAdapters.h"
#include "util/AsyncExecutor.h"

#include <QPolygon>

#include <cmath>
#include <memory>
#include <vector>

using AsyncExecutor::runAsyncTask;
using namespace SurfaceLocalizationAdapters;

namespace
{
std::vector<cv::Point> toCvPoints(const QVector<QPoint>& points)
{
  std::vector<cv::Point> result;
  result.reserve(static_cast<size_t>(points.size()));
  for (const QPoint& point : points)
  {
    result.emplace_back(point.x(), point.y());
  }
  return result;
}
}

void MainWindowSurfaceModule::testSurfaceAnnulusLocalization(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);

  if (annulus.method == "edge")
  {
    if (!annulus.hasEdgeCircle || annulus.edgeRadius <= annulus.edgeBandInner)
    {
      log(tr("log.surfaceEdgeCircleMissing") + ": " + camera.id);
      return;
    }
  }
  else if (!annulus.hasOuterCircle || !annulus.hasInnerCircle || annulus.innerRadius <= 0 || annulus.outerRadius <= annulus.innerRadius)
  {
    log(tr("log.surfaceAnnulusMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  SurfaceAnnulusThresholdConfig processorConfig;
  if (annulus.method == "edge")
  {
    processorConfig.center = cv::Point(annulus.edgeCenter.x(), annulus.edgeCenter.y());
    processorConfig.outerRadius = annulus.edgeRadius + annulus.edgeBandOuter;
    processorConfig.innerRadius = qMax(1, annulus.edgeRadius - annulus.edgeBandInner);
  }
  else
  {
    processorConfig.center = cv::Point(annulus.center.x(), annulus.center.y());
    processorConfig.outerRadius = annulus.outerRadius;
    processorConfig.innerRadius = annulus.innerRadius;
  }
  processorConfig.threshold.minValue = annulus.thresholdMin;
  processorConfig.threshold.maxValue = annulus.thresholdMax;
  processorConfig.edgeSensitivity = annulus.edgeSensitivity;
  processorConfig.edgeFitMaxError = annulus.edgeFitMaxError;

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  const bool createDiagnosticImage = camera.id == selectedCameraId() && *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;

  auto job = [input, processorConfig, exclusionRects, annulus, createDiagnosticImage]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    if (annulus.method == "edge")
    {
      return processor.locateAnnulusByEdge(input, processorConfig, toCvRects(exclusionRects), createDiagnosticImage);
    }
    return processor.locateAnnulusByGrayscaleThreshold(input, processorConfig, toCvRects(exclusionRects), createDiagnosticImage);
  };
  const QString __pendingCameraId_testSurfaceAnnulus = camera.id;
  const int setupFrameIndex = cameraRuntime()[camera.id].frameIndex();
  context().incPendingJobs(__pendingCameraId_testSurfaceAnnulus);
  runAsyncTask(decltype(job)(job), window(), [this, camera, annulus, exclusionRects, __pendingCameraId_testSurfaceAnnulus, setupFrameIndex](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceAnnulus](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceAnnulus); });
    if (*context().setupCameraId == camera.id && cameraRuntime()[camera.id].frameIndex() != setupFrameIndex)
    {
      return;
    }

    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;
    if (!result.processed || (updateView && result.diagnosticImage.empty()))
    {
      log(tr("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (updateView)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setExclusionRects(exclusionRects);
      if (*context().setupCameraId != camera.id)
      {
        largeImage()->clearCircles();
      }
    }

    if (result.blobs.empty())
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(tr("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(annulus.thresholdMin)
                  .arg(annulus.thresholdMax));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    if (result.localization.found)
    {
      context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
      cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
      if (updateView)
      {
        largeImage()->setDetectedCircle({
          QPoint(
            qRound(result.localization.center.x),
            qRound(result.localization.center.y)),
          qRound(result.localization.radius)
        });
      }
      if (context().setup)
      {
        context().setup->refreshSetupGeometryResults(camera);
        return;
      }

      if (updateView)
      {
        GeometryOverlay overlay;
        context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
        largeImage()->setGeometryOverlay(overlay);
      }
    }
    else
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearDetectedCircle();
        largeImage()->clearGeometryOverlay();
      }
    }

    log(QString("%1: %2 cx=%3 cy=%4 r=%5 score=%6 area=%7 blobs=%8 min=%9 max=%10")
                .arg(tr("log.surfaceFound"))
                .arg(camera.id)
                .arg(result.localization.found ? result.localization.center.x : mainBlob.center.x, 0, 'f', 1)
                .arg(result.localization.found ? result.localization.center.y : mainBlob.center.y, 0, 'f', 1)
                .arg(result.localization.radius, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(annulus.thresholdMin)
                .arg(annulus.thresholdMax));
  });
}

void MainWindowSurfaceModule::testSurfaceLocalization(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  ensureMassPcaReferenceFromSample(camera);

  QRect roi;
  const QVector<QPoint> searchPolygon = recipes().loadSurfaceDefectPolygon(camera.id);
  const bool hasPolygon = searchPolygon.size() >= 3;
  if (!recipes().loadSurfaceDefectRoi(camera.id, roi) && !hasPolygon)
  {
    log(tr("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }
  if (!roi.isValid() && hasPolygon)
  {
    roi = QPolygon(searchPolygon).boundingRect().normalized();
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const SurfaceDefectSettings recipeSettings = recipes().loadSurfaceDefectSettings(camera.id);
  SurfaceThresholdSettings thresholdSettings = thresholdSettingsFromRecipe(recipeSettings);

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  const bool usingSampleImage =
    cameraRuntime().find(camera.id) == cameraRuntime().end() ||
    cameraRuntime().at(camera.id).currentFrame().empty();

  // Il ROI ricetta e' fisso in immagine: con pezzo ruotato taglia il profilo e sposta il centro.
  // L'area di ricerca resta l'intero frame; il ROI serve solo come guida visiva in UI.
  const cv::Rect searchRect(0, 0, input.cols, input.rows);
  const std::vector<cv::Point> cvSearchPolygon = toCvPoints(searchPolygon);
  const bool useSearchPolygon = hasPolygon;

  const bool createDiagnosticImage = camera.id == selectedCameraId() && *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;

  auto job = [input, searchRect, cvSearchPolygon, useSearchPolygon, exclusionRects, thresholdSettings, createDiagnosticImage]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    if (useSearchPolygon)
    {
      return processor.detectByGrayscaleThreshold(
        input,
        cvSearchPolygon,
        toCvRects(exclusionRects),
        thresholdSettings,
        createDiagnosticImage);
    }
    return processor.detectByGrayscaleThreshold(
      input,
      searchRect,
      toCvRects(exclusionRects),
      thresholdSettings,
      createDiagnosticImage);
  };

  const QString __pendingCameraId_testSurfaceLocalization = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceLocalization);
  runAsyncTask(decltype(job)(job), window(), [this, camera, roi, searchPolygon, exclusionRects, thresholdSettings, usingSampleImage, __pendingCameraId_testSurfaceLocalization](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalization](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceLocalization); });
    const bool runMode = context().machineRunning != nullptr && *context().machineRunning;
    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;
    if (!result.processed || (updateView && !runMode && result.diagnosticImage.empty()))
    {
      log(tr("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (updateView)
    {
      const cv::Mat& liveFrame = cameraRuntime()[camera.id].currentFrame();
      const cv::Mat& displayImage =
        runMode && !liveFrame.empty()
          ? liveFrame
          : result.diagnosticImage;
      if (!displayImage.empty())
      {
        selectedPreview() = context().imaging->matToPixmap(displayImage);
        largeImage()->setImage(selectedPreview());
      }
      if (*context().setupCameraId != camera.id)
      {
        largeImage()->clearRoi();
        largeImage()->setSearchPolygon({});
      }
      else
      {
        largeImage()->setRoi(roi);
        largeImage()->setSearchPolygon(searchPolygon);
      }
      largeImage()->setExclusionRects(exclusionRects);
    }

    if (result.blobs.empty())
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(QString("%1: %2 blobs=0 min=%3 max=%4")
                  .arg(tr("log.surfaceNotFound"))
                  .arg(camera.id)
                  .arg(thresholdSettings.minValue)
                  .arg(thresholdSettings.maxValue));
      return;
    }

    const SurfaceBlob& mainBlob = result.blobs.front();
    if (result.localization.found)
    {
      if (usingSampleImage && result.localization.hasAxisReference)
      {
        QString saveError;
        recipes().saveSurfaceDefectAxisReference(
          camera.id,
          result.localization.referencePositiveHalfPlane,
          &saveError);
      }

      context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
      cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
      if (context().setup)
      {
        context().setup->refreshSetupGeometryResults(camera);
        return;
      }

      GeometryOverlay overlay;
      context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
      largeImage()->setGeometryOverlay(overlay);
    }
    else
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
    }

    log(QString("%1: %2 cx=%3 cy=%4 area=%5 blobs=%6 min=%7 max=%8")
                .arg(tr("log.surfaceFound"))
                .arg(camera.id)
                .arg(result.localization.found ? result.localization.center.x : mainBlob.center.x, 0, 'f', 1)
                .arg(result.localization.found ? result.localization.center.y : mainBlob.center.y, 0, 'f', 1)
                .arg(mainBlob.area, 0, 'f', 1)
                .arg(result.blobs.size())
                .arg(thresholdSettings.minValue)
                .arg(thresholdSettings.maxValue));
  });
}

void MainWindowSurfaceModule::testSurfaceLocalizationStrategy(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const SurfaceLocalizationStrategyConfig recipeStrategy = recipes().loadSurfaceLocalizationStrategy(camera.id);

  if (recipeStrategy.name != "two_circles_axis" || recipeStrategy.features.size() < 2)
  {
    log(tr("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  auto processorStrategy = toProcessorStrategy(recipeStrategy);

  const bool createDiagnosticImage =
    camera.id == selectedCameraId() &&
    *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry &&
    *context().setupCameraId != camera.id;

  auto job = [input, processorStrategy, exclusionRects, createDiagnosticImage]() -> SurfaceStrategyResult {
    SurfaceDefectProcessor processor;
    return processor.locateTwoCirclesAxis(input, processorStrategy, toCvRects(exclusionRects), createDiagnosticImage);
  };

  const QString __pendingCameraId_testSurfaceLocalizationStrategy = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy);
  runAsyncTask(decltype(job)(job), window(), [this, camera, exclusionRects, __pendingCameraId_testSurfaceLocalizationStrategy](const SurfaceStrategyResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceLocalizationStrategy](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceLocalizationStrategy); });
    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry &&
      *context().setupCameraId != camera.id;
    if (!result.processed || (updateView && result.diagnosticImage.empty()))
    {
      log(tr("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (updateView)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setExclusionRects(exclusionRects);
    }

    if (!result.found)
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    SurfaceLocalizationReference reference;
    reference.found = true;
    reference.method = result.strategyName;
    reference.center = result.origin;
    reference.angleRadians = std::atan2(
      result.xAxisEnd.y - result.xAxisStart.y,
      result.xAxisEnd.x - result.xAxisStart.x);
    reference.score = 1.0;
    reference.inputPoints = static_cast<int>(result.features.size());
    reference.usedPoints = static_cast<int>(result.features.size());
    reference.xAxisStart = result.xAxisStart;
    reference.xAxisEnd = result.xAxisEnd;
    reference.yAxisStart = result.yAxisStart;
    reference.yAxisEnd = result.yAxisEnd;
    context().lastSurfaceLocalizationResults->insert(camera.id, reference);
    cameraRuntime()[camera.id].setCurrentPose(
      context().imaging->partPoseFromSurfaceReference(camera, reference));
    if (context().setup)
    {
      context().setup->refreshSetupGeometryResults(camera);
    }

    if (updateView)
    {
      GeometryOverlay overlay;
      context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
      largeImage()->setGeometryOverlay(overlay);
    }

    log(QString("%1: %2 originX=%3 originY=%4 features=%5")
                .arg(tr("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.origin.x, 0, 'f', 1)
                .arg(result.origin.y, 0, 'f', 1)
                .arg(result.features.size()));
  });
}

void MainWindowSurfaceModule::testSurfaceEdgePcaLocalization(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    log(tr("log.surfaceNotAvailable") + ": " + camera.id);
    return;
  }

  QRect roi;
  const QVector<QPoint> searchPolygon = recipes().loadSurfaceDefectPolygon(camera.id);
  const bool hasPolygon = searchPolygon.size() >= 3;
  if (!recipes().loadSurfaceDefectRoi(camera.id, roi) && !hasPolygon)
  {
    log(tr("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }
  if (!roi.isValid() && hasPolygon)
  {
    roi = QPolygon(searchPolygon).boundingRect().normalized();
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  const SurfaceDefectSettings recipeSettings = recipes().loadSurfaceDefectSettings(camera.id);
  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  const cv::Rect searchRect(0, 0, input.cols, input.rows);
  const std::vector<cv::Point> cvSearchPolygon = toCvPoints(searchPolygon);
  const bool useSearchPolygon = hasPolygon;
  const bool resolveAmbiguity =
    annulus.pcaResolveAmbiguity || recipeSettings.pcaResolveAmbiguity;

  const bool createDiagnosticImage = camera.id == selectedCameraId() && *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;

  auto job = [input, searchRect, cvSearchPolygon, useSearchPolygon, exclusionRects, annulus, resolveAmbiguity, createDiagnosticImage]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    if (useSearchPolygon)
    {
      return processor.locateByEdgePca(
        input,
        cvSearchPolygon,
        toCvRects(exclusionRects),
        annulus.edgeSensitivity,
        resolveAmbiguity,
        createDiagnosticImage);
    }
    return processor.locateByEdgePca(
      input,
      searchRect,
      toCvRects(exclusionRects),
      annulus.edgeSensitivity,
      resolveAmbiguity,
      createDiagnosticImage);
  };

  const QString __pendingCameraId_testSurfaceEdgePca = camera.id;
  const int setupFrameIndex = cameraRuntime()[camera.id].frameIndex();
  context().incPendingJobs(__pendingCameraId_testSurfaceEdgePca);
  runAsyncTask(decltype(job)(job), window(), [this, camera, roi, searchPolygon, exclusionRects, __pendingCameraId_testSurfaceEdgePca, setupFrameIndex](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceEdgePca](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceEdgePca); });
    if (*context().setupCameraId == camera.id && cameraRuntime()[camera.id].frameIndex() != setupFrameIndex)
    {
      return;
    }

    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry;
    if (!result.processed || (updateView && result.diagnosticImage.empty()))
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(tr("log.surfaceFailed") + ": " + camera.id);
      return;
    }

    if (updateView)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      if (*context().setupCameraId != camera.id)
      {
        largeImage()->clearRoi();
        largeImage()->setSearchPolygon({});
      }
      else
      {
        largeImage()->setRoi(roi);
        largeImage()->setSearchPolygon(searchPolygon);
      }
      largeImage()->setExclusionRects(exclusionRects);
      largeImage()->clearCircles();
    }

    if (!result.localization.found)
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (updateView)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
    cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
    if (context().setup)
    {
      context().setup->refreshSetupGeometryResults(camera);
    }

    GeometryOverlay overlay;
    context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    if (updateView)
    {
      largeImage()->setGeometryOverlay(overlay);
    }
    log(QString("%1: %2 cx=%3 cy=%4 angle=%5 score=%6")
                .arg(tr("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

