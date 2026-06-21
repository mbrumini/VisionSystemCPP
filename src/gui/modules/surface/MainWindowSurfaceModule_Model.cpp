#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
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

namespace
{
QRect fullImageRoi(const cv::Mat& image)
{
  return QRect(0, 0, image.cols, image.rows);
}

cv::Rect toCvRect(const QRect& rect)
{
  return cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

SurfaceShapeMatchConfig shapeMatchConfigFromModel(const SurfaceModelConfig& model, const cv::Mat& input)
{
  SurfaceShapeMatchConfig config;
  config.searchRoi = cv::Rect(0, 0, input.cols, input.rows);
  config.modelContour.reserve(static_cast<size_t>(model.contour.size()));
  for (const QPoint& point : model.contour)
  {
    config.modelContour.emplace_back(point.x(), point.y());
  }
  config.edgeSensitivity = model.edgeSensitivity;
  config.maxShapeDistance = model.maxShapeDistance;
  config.useConvexHull = model.useConvexHull;
  return config;
}
}

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
  const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
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
    current.edgeSensitivity,
    current.useConvexHull);

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
  restoreSurfaceModelPoseFromSample(camera);
  log(QString("%1: %2 points=%3").arg(tr("actions.acquireModel")).arg(camera.id).arg(contour.size()));
}

bool MainWindowSurfaceModule::restoreSurfaceModelPoseFromSample(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    return false;
  }

  const SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
  if (!model.hasModel || model.contour.isEmpty())
  {
    return false;
  }

  QString imageError;
  const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
  if (input.empty())
  {
    return false;
  }

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  SurfaceDefectProcessor processor;
  const SurfaceDefectResult result = processor.locateByShapeMatching(
    input,
    shapeMatchConfigFromModel(model, input),
    toCvRects(exclusionRects));

  if (!result.processed || !result.localization.found)
  {
    return false;
  }

  context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
  cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
  return true;
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
  const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
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
    current.edgeSensitivity,
    current.useConvexHull);

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
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
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
  const cv::Mat input = context().imaging->validationInputImage(camera, &imageError);
  if (input.empty() || model.contour.isEmpty())
  {
    log(input.empty() ? imageError : tr("surfaceModel.missing"));
    return;
  }

  const QRect searchRoi = fullImageRoi(input);
  SurfaceShapeMatchConfig config = shapeMatchConfigFromModel(model, input);

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByShapeMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceShapeModel = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceShapeModel);
  runAsyncTask(decltype(job)(job), window(), [this, camera, searchRoi, exclusionRects, __pendingCameraId_testSurfaceShapeModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceShapeModel](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceShapeModel); });
    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry &&
      *context().setupCameraId != camera.id;
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
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
      return;
    }

    if (updateView)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setRoi(searchRoi);
      largeImage()->setExclusionRects(exclusionRects);
      largeImage()->clearCircles();
    }
    GeometryOverlay overlay;
    context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    if (updateView)
    {
      largeImage()->setGeometryOverlay(overlay);
    }
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
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
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
  const cv::Mat input = context().imaging->validationInputImage(camera, &imageError);
  const cv::Mat modelImage = cv::imread(model.templateImagePath.toStdString(), cv::IMREAD_COLOR);
  if (input.empty() || modelImage.empty())
  {
    log(input.empty() ? imageError : tr("log.imageMissing") + ": " + model.templateImagePath);
    return;
  }

  SurfaceTemplateMatchConfig config;
  const QRect searchRoi = fullImageRoi(input);
  config.searchRoi = toCvRect(searchRoi);
  config.modelImage = modelImage;
  config.edgeSensitivity = model.edgeSensitivity;
  config.minScore = model.minTemplateScore;
  config.angleStartDegrees = model.angleStartDegrees;
  config.angleEndDegrees = model.angleEndDegrees;
  config.angleStepDegrees = model.angleStepDegrees;
  config.useEdges = model.modelUseEdges;

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);

  auto job = [input, config, exclusionRects]() -> SurfaceDefectResult {
    SurfaceDefectProcessor processor;
    return processor.locateByTemplateMatching(input, config, toCvRects(exclusionRects));
  };

  const QString __pendingCameraId_testSurfaceTemplateModel = camera.id;
  context().incPendingJobs(__pendingCameraId_testSurfaceTemplateModel);
  runAsyncTask(decltype(job)(job), window(), [this, camera, searchRoi, exclusionRects, __pendingCameraId_testSurfaceTemplateModel](const SurfaceDefectResult& result) {
    auto __dec_guard = std::shared_ptr<void>(nullptr, [this, __pendingCameraId_testSurfaceTemplateModel](void*) { context().decPendingJobs(__pendingCameraId_testSurfaceTemplateModel); });
    const bool updateView =
      camera.id == selectedCameraId() &&
      *context().activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry &&
      *context().setupCameraId != camera.id;
    if (!result.processed || result.diagnosticImage.empty() || !result.localization.found)
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
      return;
    }

    if (updateView)
    {
      selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
      largeImage()->setImage(selectedPreview());
      largeImage()->setRoi(searchRoi);
      largeImage()->setExclusionRects(exclusionRects);
      largeImage()->clearCircles();
    }
    GeometryOverlay overlay;
    context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
    if (updateView)
    {
      largeImage()->setGeometryOverlay(overlay);
    }
    log(QString("%1: %2 template cx=%3 cy=%4 angle=%5 score=%6")
                .arg(tr("log.surfaceStrategyFound"))
                .arg(camera.id)
                .arg(result.localization.center.x, 0, 'f', 1)
                .arg(result.localization.center.y, 0, 'f', 1)
                .arg(result.localization.angleRadians * 180.0 / CV_PI, 0, 'f', 1)
                .arg(result.localization.score, 0, 'f', 2));
  });
}

