#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowSetupModule.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/setup/AiLocalizationPaths.h"

#include "gui/ImagePrimitives.h"

#include "gui/geometry/GeometryOverlay.h"
#include "gui/SurfaceLocalizationAdapters.h"
#include "processing/SurfaceModelTrainer.h"
#include "util/AsyncExecutor.h"

#include <QDir>
#include <QFileInfo>
#include <QPolygon>

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

cv::RotatedRect toCvRotatedRect(const RecipeRotatedRoi& roi)
{
  return cv::RotatedRect(
    cv::Point2f(static_cast<float>(roi.center.x()), static_cast<float>(roi.center.y())),
    cv::Size2f(static_cast<float>(roi.size.width()), static_cast<float>(roi.size.height())),
    static_cast<float>(roi.angleDegrees));
}

ImageRotatedRect toImageRotatedRect(const RecipeRotatedRoi& roi)
{
  return ImageRotatedRect{roi.center, roi.size, roi.angleDegrees};
}

RecipeRotatedRoi loadModelSearchRoi(const RecipeManager& recipes, const QString& cameraId)
{
  RecipeRotatedRoi roi;
  if (recipes.loadSurfaceDefectRotatedRoi(cameraId, roi))
  {
    return roi;
  }

  QRect box;
  if (recipes.loadSurfaceDefectRoi(cameraId, box))
  {
    roi.center = box.center() + QPointF(0.5, 0.5);
    roi.size = box.size();
    roi.valid = true;
  }
  return roi;
}

QRect boundingRectFromRotatedRoi(const RecipeRotatedRoi& roi)
{
  const cv::RotatedRect fitted = toCvRotatedRect(roi);
  const cv::Rect bounds = fitted.boundingRect();
  return QRect(bounds.x, bounds.y, bounds.width, bounds.height);
}

void applyTemplateMatchAngleReference(
  SurfaceTemplateMatchConfig& config,
  const SurfaceModelConfig& model,
  const RecipeManager& recipes,
  const QString& cameraId)
{
  if (model.hasReferenceAngle)
  {
    config.hasReferenceAngle = true;
    config.referenceAngleDegrees = model.referenceAngleDegrees;
    return;
  }

  RecipeRotatedRoi roi;
  if (recipes.loadSurfaceDefectRotatedRoi(cameraId, roi) && roi.valid)
  {
    config.hasReferenceAngle = true;
    config.referenceAngleDegrees = roi.angleDegrees;
  }
}

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

cv::Rect sampleSearchRect(const CameraConfig& camera, const cv::Mat& input, const RecipeManager& recipes)
{
  QRect globalAoi;
  if (recipes.loadGlobalSurfaceAoi(camera.id, globalAoi))
  {
    return cv::Rect(globalAoi.x(), globalAoi.y(), globalAoi.width(), globalAoi.height());
  }

  return cv::Rect(0, 0, input.cols, input.rows);
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

bool MainWindowSurfaceModule::localizePoseOnSample(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    return false;
  }

  QString imageError;
  const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
  if (input.empty())
  {
    return false;
  }

  const QString modelPath = aiNewestLocalizationModelPath(recipes(), camera.id);
  if (recipes().loadAiLocalizationEnabled(camera.id) &&
      !modelPath.isEmpty() &&
      QFileInfo::exists(modelPath) &&
      context().setup)
  {
    return context().setup->runAiLocalizationInferenceSync(camera, input);
  }

  const SurfaceAnnulusLocalizationConfig annulus = recipes().loadSurfaceAnnulusLocalization(camera.id);
  const SurfaceDefectSettings recipeSettings = recipes().loadSurfaceDefectSettings(camera.id);
  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  const QVector<QPoint> searchPolygon = recipes().loadSurfaceDefectPolygon(camera.id);
  const bool hasPolygon = searchPolygon.size() >= 3;
  QRect configuredRoi;
  recipes().loadSurfaceDefectRoi(camera.id, configuredRoi);
  const cv::Rect fullSearchRect(0, 0, input.cols, input.rows);
  const cv::Rect edgePcaSearchRect = configuredRoi.isValid()
    ? (toCvRect(configuredRoi) & fullSearchRect)
    : fullSearchRect;
  const std::vector<cv::Point> cvSearchPolygon = toCvPoints(searchPolygon);
  const bool resolveAmbiguity = annulus.pcaResolveAmbiguity || recipeSettings.pcaResolveAmbiguity;
  SurfaceDefectProcessor processor;
  SurfaceDefectResult result;

  if (annulus.method == "model")
  {
    return restoreSurfaceModelPoseFromSample(camera);
  }

  if (annulus.method == "edgePca")
  {
    if (hasPolygon)
    {
      result = processor.locateByEdgePca(
        input,
        cvSearchPolygon,
        toCvRects(exclusionRects),
        annulus.edgeSensitivity,
        resolveAmbiguity,
        false);
    }
    else
    {
      result = processor.locateByEdgePca(
        input,
        edgePcaSearchRect,
        toCvRects(exclusionRects),
        annulus.edgeSensitivity,
        resolveAmbiguity,
        false);
    }
  }
  else if (annulus.method == "massPca")
  {
    ensureMassPcaReferenceFromSample(camera);
    const SurfaceThresholdSettings thresholdSettings = thresholdSettingsFromRecipe(recipeSettings);
    result = processor.detectByGrayscaleThreshold(
      input,
      fullSearchRect,
      toCvRects(exclusionRects),
      thresholdSettings,
      false);
  }
  else
  {
    if (hasPolygon)
    {
      result = processor.locateByEdgePca(
        input,
        cvSearchPolygon,
        toCvRects(exclusionRects),
        annulus.edgeSensitivity,
        resolveAmbiguity,
        false);
    }
    else
    {
      result = processor.locateByEdgePca(
        input,
        edgePcaSearchRect,
        toCvRects(exclusionRects),
        annulus.edgeSensitivity,
        resolveAmbiguity,
        false);
    }
  }

  if (!result.processed || !result.localization.found)
  {
    return false;
  }

  context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
  cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
  syncThreadExtractionRoiOverlay(camera);
  return true;
}

bool MainWindowSurfaceModule::ensureMassPcaReferenceFromSample(const CameraConfig& camera)
{
  const SurfaceDefectSettings recipeSettings = recipes().loadSurfaceDefectSettings(camera.id);
  if (recipeSettings.hasReferenceHalfPlane && recipeSettings.hasReferenceArea)
  {
    return true;
  }

  QString imageError;
  const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
  if (input.empty())
  {
    return false;
  }

  SurfaceThresholdSettings thresholdSettings;
  thresholdSettings.minValue = recipeSettings.thresholdMin;
  thresholdSettings.maxValue = recipeSettings.thresholdMax;

  const QVector<QRect> exclusionRects = recipes().loadSurfaceDefectExclusionRects(camera.id);
  SurfaceDefectProcessor processor;
  const cv::Rect searchRect(0, 0, input.cols, input.rows);
  const SurfaceDefectResult result = processor.detectByGrayscaleThreshold(
    input,
    searchRect,
    toCvRects(exclusionRects),
    thresholdSettings,
    false,
    false);

  if (!result.localization.found)
  {
    return false;
  }

  QString error;
  return recipes().saveSurfaceDefectMassPcaReference(
    camera.id,
    result.localization.referencePositiveHalfPlane,
    result.totalArea,
    &error);
}

void MainWindowSurfaceModule::showSamplePoseOverlay(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  if (!localizePoseOnSample(camera))
  {
    largeImage()->clearGeometryOverlay();
    log(tr("log.localizationNotFound") + ": " + camera.id);
    return;
  }

  if (!context().geometry)
  {
    return;
  }

  GeometryOverlay overlay;
  context().geometry->appendCurrentPartPoseOverlay(camera, overlay);
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowSurfaceModule::acquireSurfaceModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  const RecipeRotatedRoi searchRoi = loadModelSearchRoi(recipes(), camera.id);
  if (!searchRoi.valid)
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
  const SurfaceModelTrainingResult training = trainer.trainFromRotatedRoi(
    input,
    toCvRotatedRect(searchRoi),
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
  const QRect modelBounds = boundingRectFromRotatedRoi(searchRoi);
  if (!recipes().saveSurfaceModel(camera.id, modelBounds, contour, templatePath, &error, searchRoi.angleDegrees))
  {
    log(error);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(training.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  largeImage()->clearRoi();
  largeImage()->setGeometryArea(toImageRotatedRect(searchRoi));
  largeImage()->setExclusionRects(exclusionRects);
  showSamplePoseOverlay(camera);
  log(QString("%1: %2 points=%3").arg(tr("actions.acquireModel")).arg(camera.id).arg(contour.size()));
}

bool MainWindowSurfaceModule::restoreSurfaceModelPoseFromSample(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()))
  {
    return false;
  }

  const QString modelPath = aiNewestLocalizationModelPath(recipes(), camera.id);
  if (recipes().loadAiLocalizationEnabled(camera.id) &&
      !modelPath.isEmpty() &&
      QFileInfo::exists(modelPath) &&
      context().setup)
  {
    QString imageError;
    const cv::Mat input = context().imaging->sampleInputImage(camera, &imageError);
    if (input.empty())
    {
      return false;
    }
    return context().setup->runAiLocalizationInferenceSync(camera, input);
  }

  const SurfaceModelConfig model = recipes().loadSurfaceModel(camera.id);
  if (!model.hasModel)
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
  SurfaceDefectResult result;

  if (model.modelType == "template")
  {
    const QString templatePath = recipes().surfaceModelTemplateImagePath(camera.id);
    const cv::Mat templateImage = cv::imread(templatePath.toStdString(), cv::IMREAD_COLOR);
    if (templateImage.empty())
    {
      return false;
    }

    SurfaceTemplateMatchConfig config;
    config.searchRoi = sampleSearchRect(camera, input, recipes());
    config.modelImage = templateImage;
    config.edgeSensitivity = model.edgeSensitivity;
    config.minScore = model.minTemplateScore;
    config.angleStartDegrees = model.angleStartDegrees;
    config.angleEndDegrees = model.angleEndDegrees;
    config.angleStepDegrees = model.angleStepDegrees;
    config.useEdges = model.modelUseEdges;
    applyTemplateMatchAngleReference(config, model, recipes(), camera.id);

    result = processor.locateByTemplateMatching(
      input,
      config,
      toCvRects(exclusionRects));
  }
  else
  {
    if (model.contour.isEmpty())
    {
      return false;
    }

    result = processor.locateByShapeMatching(
      input,
      shapeMatchConfigFromModel(model, input),
      toCvRects(exclusionRects));
  }

  if (!result.processed || !result.localization.found)
  {
    return false;
  }

  context().lastSurfaceLocalizationResults->insert(camera.id, result.localization);
  cameraRuntime()[camera.id].setCurrentPose(context().imaging->partPoseFromSurfaceReference(camera, result.localization));
  syncThreadExtractionRoiOverlay(camera);
  return true;
}

void MainWindowSurfaceModule::previewSurfaceModel(const CameraConfig& camera)
{
  if (!MainWindowCameraProfile::isGrayscaleLocalization(camera, config()) || camera.id != selectedCameraId())
  {
    return;
  }

  const RecipeRotatedRoi searchRoi = loadModelSearchRoi(recipes(), camera.id);
  if (!searchRoi.valid)
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
  const SurfaceModelTrainingResult training = trainer.trainFromRotatedRoi(
    input,
    toCvRotatedRect(searchRoi),
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
  largeImage()->clearRoi();
  largeImage()->setGeometryArea(toImageRotatedRect(searchRoi));
  largeImage()->setExclusionRects(exclusionRects);
  showSamplePoseOverlay(camera);
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
    syncThreadExtractionRoiOverlay(camera);
    if (isSetupCameraActive(camera.id))
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

  const cv::Rect cvSearchRoi = sampleSearchRect(camera, input, recipes());
  const QRect searchRoi(cvSearchRoi.x, cvSearchRoi.y, cvSearchRoi.width, cvSearchRoi.height);
  SurfaceTemplateMatchConfig config;
  config.searchRoi = cvSearchRoi;
  config.modelImage = modelImage;
  config.edgeSensitivity = model.edgeSensitivity;
  config.minScore = model.minTemplateScore;
  config.angleStartDegrees = model.angleStartDegrees;
  config.angleEndDegrees = model.angleEndDegrees;
  config.angleStepDegrees = model.angleStepDegrees;
  config.useEdges = model.modelUseEdges;
  applyTemplateMatchAngleReference(config, model, recipes(), camera.id);

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
    syncThreadExtractionRoiOverlay(camera);
    if (isSetupCameraActive(camera.id))
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

