#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowContext.h"

#include "gui/SurfaceLocalizationAdapters.h"
#include "processing/SurfaceModelTrainer.h"
#include "util/AsyncExecutor.h"

#include <QDir>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

#include <memory>

using AsyncExecutor::runAsyncTask;
using namespace SurfaceLocalizationAdapters;

void MainWindowSurfaceModule::acquireSurfaceModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  QRect roi;
  if (!recipes().loadSurfaceDefectRoi(camera.id, roi))
  {
    log(tr("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const SurfaceModelConfig current = recipes().loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (!training.trained || training.templateImage.empty() || training.contour.empty())
  {
    log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  const QString templatePath = recipes().surfaceModelTemplateImagePath(camera.id);
  QDir().mkpath(QFileInfo(templatePath).dir().absolutePath());
  if (!cv::imwrite(templatePath.toStdString(), training.templateImage))
  {
    log("Impossibile salvare template modello: " + templatePath);
    return;
  }

  QVector<QPoint> contour;
  contour.reserve(static_cast<int>(training.contour.size()));
  for (const cv::Point& point : training.contour)
  {
    contour.append(QPoint(point.x, point.y));
  }

  QString error;
  if (!recipes().saveSurfaceModel(camera.id, roi, contour, templatePath, &error))
  {
    log(error);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(training.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  largeImage()->setRoi(roi);
  largeImage()->setExclusionRects(exclusionRects);
  log(QString("%1: %2 points=%3").arg(tr("actions.acquireModel")).arg(camera.id).arg(contour.size()));
}

void MainWindowSurfaceModule::previewSurfaceModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  QRect roi;
  if (!recipes().loadSurfaceDefectRoi(camera.id, roi))
  {
    log(tr("log.surfaceRoiMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const SurfaceModelConfig current = recipes().loadSurfaceModel(camera.id);
  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  SurfaceModelTrainer trainer;
  const SurfaceModelTrainingResult training = trainer.trainFromRoi(
    input,
    cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
    toCvRects(exclusionRects),
    current.edgeSensitivity);

  if (training.diagnosticImage.empty())
  {
    log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(training.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  largeImage()->setRoi(roi);
  largeImage()->setExclusionRects(exclusionRects);
  log(QString("%1: %2 points=%3")
              .arg(tr("actions.previewModel"))
              .arg(camera.id)
              .arg(training.contour.size()));
}

void MainWindowSurfaceModule::testSurfaceShapeModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  const SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    log(tr("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty() || model.contour.isEmpty())
  {
    log(input.empty() ? imageError : tr("surfaceModel.missing"));
    return;
  }

  SurfaceShapeMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelContour.reserve(static_cast<size_t>(model.contour.size()));
  for (const QPoint& point : model.contour)
  {
    config.modelContour.emplace_back(point.x(), point.y());
  }
  config.edgeSensitivity = model.edgeSensitivity;
  config.maxShapeDistance = model.maxShapeDistance;

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByShapeMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceShapeModel = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceShapeModel);
  runAsyncTask(decltype(job)(job), window(), [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceShapeModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceShapeModel](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceShapeModel); });
    const bool suppressViewUpdate =
      camera.id == selectedCameraId() &&
      (*context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry || *context().setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
    cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
    if (!suppressViewUpdate)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setRoi(model.searchRoi);
      largeImage()->setExclusionRects(exclusionRects);
      largeImage()->clearCircles();
    }
    GeometryOverlay overlay;
    context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    largeImage()->setGeometryOverlay(overlay);
    log(QString("%1: %2 shape cx=%3 cy=%4 angle=%5 score=%6")
                .arg(tr("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

void MainWindowSurfaceModule::testSurfaceTemplateModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  const SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
  if (!model.hasModel)
  {
    log(tr("log.surfaceStrategyMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  const cv::Mat modelImage = cv::imread(model.templateImagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() || modelImage.empty())
  {
    log(input.empty() ? imageError : tr("log.imageMissing") + ": " + model.templateImagePath);
    return;
  }

  SurfaceTemplateMatchConfig config;
  config.searchRoi = cv::Rect(model.searchRoi.x(), model.searchRoi.y(), model.searchRoi.width(), model.searchRoi.height());
  config.modelImage = modelImage;
  config.edgeSensitivity = model.edgeSensitivity;
  config.minScore = model.minTemplateScore;
  config.angleStartDegrees = model.angleStartDegrees;
  config.angleEndDegrees = model.angleEndDegrees;
  config.angleStepDegrees = model.angleStepDegrees;

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByTemplateMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceTemplateModel = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceTemplateModel);
  runAsyncTask(decltype(job)(job), window(), [this, camera, model, exclusionRects, __pendingCameraId_testSurfaceTemplateModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceTemplateModel](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceTemplateModel); });
    const bool suppressViewUpdate =
      camera.id == selectedCameraId() &&
      (*context().activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry || *context().setupCameraId == camera.id);
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
    {
      context().lastSurfaceLocalizationResults->remove(camera.id);
      cameraRuntime()[camera.id].clearCurrentPose(camera.id);
      if (!suppressViewUpdate)
      {
        largeImage()->clearGeometryOverlay();
      }
      log(tr("log.surfaceStrategyNotFound") + ": " + camera.id);
      return;
    }

    context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
    cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
    if (!suppressViewUpdate)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setRoi(model.searchRoi);
      largeImage()->setExclusionRects(exclusionRects);
      largeImage()->clearCircles();
    }
    GeometryOverlay overlay;
    context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    largeImage()->setGeometryOverlay(overlay);
    log(QString("%1: %2 template cx=%3 cy=%4 angle=%5 score=%6")
                .arg(tr("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

