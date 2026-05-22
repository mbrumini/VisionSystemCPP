#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowContext.h"

#include "processing/geometry/EdgeCircleDetector.h"

#include <QColor>
#include <QPointF>

#include <algorithm>
#include <cmath>

namespace
{
double normalizedAngle(double angle)
{
  while (angle < 0.0)
  {
    angle += 2.0 * CV_PI;
  }
  while (angle >= 2.0 * CV_PI)
  {
    angle -= 2.0 * CV_PI;
  }
  return angle;
}

bool arcGuideFromThreePoints(
  const QVector<QPoint>& points,
  cv::Point2d& center,
  double& radius,
  double& startAngle,
  double& endAngle)
{
  if (points.size() < 3)
  {
    return false;
  }

  const double x1 = points[0].x();
  const double y1 = points[0].y();
  const double x2 = points[1].x();
  const double y2 = points[1].y();
  const double x3 = points[2].x();
  const double y3 = points[2].y();
  const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
  if (std::abs(d) < 0.001)
  {
    return false;
  }

  const double ux = ((x1 * x1 + y1 * y1) * (y2 - y3) +
                     (x2 * x2 + y2 * y2) * (y3 - y1) +
                     (x3 * x3 + y3 * y3) * (y1 - y2)) / d;
  const double uy = ((x1 * x1 + y1 * y1) * (x3 - x2) +
                     (x2 * x2 + y2 * y2) * (x1 - x3) +
                     (x3 * x3 + y3 * y3) * (x2 - x1)) / d;
  center = cv::Point2d(ux, uy);
  radius = std::hypot(x1 - ux, y1 - uy);
  if (radius <= 2.0)
  {
    return false;
  }

  startAngle = normalizedAngle(std::atan2(y1 - uy, x1 - ux));
  endAngle = normalizedAngle(std::atan2(y2 - uy, x2 - ux));
  const double throughAngle = normalizedAngle(std::atan2(y3 - uy, x3 - ux));
  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  double throughSpan = throughAngle - startAngle;
  while (throughSpan < 0.0)
  {
    throughSpan += 2.0 * CV_PI;
  }
  if (throughSpan > span)
  {
    std::swap(startAngle, endAngle);
  }
  return true;
}

QString transitionToRecipeLocal(EdgeLineTransition transition)
{
  return transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark";
}

EdgeLineTransition transitionFromRecipeLocal(const QString& value)
{
  return value == "dark_to_light" ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
}

QString pickModeToRecipeLocal(EdgeLinePickMode mode)
{
  if (mode == EdgeLinePickMode::Last)
  {
    return "last";
  }
  if (mode == EdgeLinePickMode::Best)
  {
    return "best";
  }
  return "first";
}

EdgeLinePickMode pickModeFromRecipeLocal(const QString& value)
{
  if (value == "last")
  {
    return EdgeLinePickMode::Last;
  }
  if (value == "best")
  {
    return EdgeLinePickMode::Best;
  }
  return EdgeLinePickMode::First;
}

QString scanDirectionToRecipeLocal(EdgeLineScanDirection direction)
{
  return direction == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive";
}

EdgeLineScanDirection scanDirectionFromRecipeLocal(const QString& value)
{
  return value == "normal_negative" ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
}

void appendArcPolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  double startAngle,
  double endAngle,
  const QColor& color,
  int width)
{
  if (radius <= 1.0)
  {
    return;
  }

  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  if (span < 0.001)
  {
    span = 2.0 * CV_PI;
  }

  const int segments = std::max(8, static_cast<int>(std::round(48.0 * span / (2.0 * CV_PI))));
  QPointF previous;
  for (int i = 0; i <= segments; ++i)
  {
    const double angle = startAngle + span * static_cast<double>(i) / static_cast<double>(segments);
    const QPointF current(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    if (i > 0)
    {
      overlay.lines.append({previous, current, color, width});
    }
    previous = current;
  }
}
}

void MainWindowGeometryModule::loadGeometryArcsRecipe(const CameraConfig& camera)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  if (!arcs.isEmpty())
  {
    return;
  }

  const QVector<GeometryArcRecipeConfig> arcRecipes = recipes().loadGeometryArcs(camera.id);
  for (const GeometryArcRecipeConfig& recipe : arcRecipes)
  {
    if (!recipe.enabled)
    {
      continue;
    }

    GeometryArcRuntimeConfig arc;
    arc.enabled = recipe.enabled;
    arc.id = recipe.id;
    arc.partCenter = cv::Point2d(recipe.partCenter.x(), recipe.partCenter.y());
    arc.partStart = cv::Point2d(recipe.partStart.x(), recipe.partStart.y());
    arc.partEnd = cv::Point2d(recipe.partEnd.x(), recipe.partEnd.y());
    arc.radius = recipe.radius;
    arc.startAngleRadians = recipe.startAngleRadians;
    arc.endAngleRadians = recipe.endAngleRadians;
    arc.innerBand = recipe.innerBand;
    arc.outerBand = recipe.outerBand;
    arc.edgeSensitivity = recipe.edgeSensitivity;
    arc.edgeCleanupDerivative = recipe.edgeCleanupDerivative;
    arc.edgeStatisticalFilter = recipe.edgeStatisticalFilter;
    arc.useSubpixel = recipe.useSubpixel;
    arc.scanDirection = scanDirectionFromRecipeLocal(recipe.scanDirection);
    arc.transition = transitionFromRecipeLocal(recipe.transition);
    arc.pickMode = pickModeFromRecipeLocal(recipe.pickMode);
    arc.hasArc = true;
    arcs.append(arc);
  }

  if (arcs.isEmpty())
  {
    GeometryArcRuntimeConfig arc;
    arc.id = "arc_1";
    arcs.append(arc);
  }
  m_activeArcIndexes[camera.id] = qBound(0, m_activeArcIndexes.value(camera.id, 0), arcs.size() - 1);
}

void MainWindowGeometryModule::saveGeometryArcsRecipe(const CameraConfig& camera)
{
  QVector<GeometryArcRecipeConfig> recipeList;
  for (const GeometryArcRuntimeConfig& arc : m_arcConfigs[camera.id])
  {
    if (!arc.hasArc)
    {
      continue;
    }

    GeometryArcRecipeConfig recipe;
    recipe.enabled = arc.enabled;
    recipe.id = arc.id;
    recipe.partCenter = QPointF(arc.partCenter.x, arc.partCenter.y);
    recipe.partStart = QPointF(arc.partStart.x, arc.partStart.y);
    recipe.partEnd = QPointF(arc.partEnd.x, arc.partEnd.y);
    recipe.radius = arc.radius;
    recipe.startAngleRadians = arc.startAngleRadians;
    recipe.endAngleRadians = arc.endAngleRadians;
    recipe.innerBand = arc.innerBand;
    recipe.outerBand = arc.outerBand;
    recipe.edgeSensitivity = arc.edgeSensitivity;
    recipe.edgeCleanupDerivative = arc.edgeCleanupDerivative;
    recipe.edgeStatisticalFilter = arc.edgeStatisticalFilter;
    recipe.useSubpixel = arc.useSubpixel;
    recipe.scanDirection = scanDirectionToRecipeLocal(arc.scanDirection);
    recipe.transition = transitionToRecipeLocal(arc.transition);
    recipe.pickMode = pickModeToRecipeLocal(arc.pickMode);
    recipeList.append(recipe);
  }

  QString error;
  if (!recipes().saveGeometryArcs(camera.id, recipeList, &error))
  {
    log(error);
  }
}

void MainWindowGeometryModule::addGeometryArc(const CameraConfig& camera)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  GeometryArcRuntimeConfig arc;
  arc.id = QString("arc_%1").arg(arcs.size() + 1);
  arcs.append(arc);
  m_activeArcIndexes[camera.id] = arcs.size() - 1;
}

void MainWindowGeometryModule::removeActiveGeometryArc(const CameraConfig& camera)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  if (arcs.isEmpty())
  {
    return;
  }

  const int index = qBound(0, m_activeArcIndexes.value(camera.id, 0), arcs.size() - 1);
  arcs.removeAt(index);
  if (arcs.isEmpty())
  {
    GeometryArcRuntimeConfig arc;
    arc.id = "arc_1";
    arcs.append(arc);
  }
  m_activeArcIndexes[camera.id] = qBound(0, index, arcs.size() - 1);
  saveGeometryArcsRecipe(camera);
  showGeometryArcPanel(camera);
}

GeometryArcRuntimeConfig& MainWindowGeometryModule::activeGeometryArcConfig(const QString& cameraId)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[cameraId];
  if (arcs.isEmpty())
  {
    GeometryArcRuntimeConfig arc;
    arc.id = "arc_1";
    arcs.append(arc);
  }
  const int index = qBound(0, m_activeArcIndexes.value(cameraId, 0), arcs.size() - 1);
  return arcs[index];
}

const GeometryArcRuntimeConfig& MainWindowGeometryModule::activeGeometryArcConfig(const QString& cameraId) const
{
  return m_arcConfigs[cameraId][qBound(0, m_activeArcIndexes.value(cameraId, 0), m_arcConfigs[cameraId].size() - 1)];
}

void MainWindowGeometryModule::activateGeometryArcDrawing(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  context().deactivateImageDrawingTools();
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Arc;
  largeImage()->setThreePointArcDrawingEnabled(true);
  log(tr("log.geometryArcDrawing") + ": " + camera.id);
}

void MainWindowGeometryModule::handleGeometryArcPoints(const CameraConfig& camera, const QVector<QPoint>& points)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  cv::Point2d center;
  double radius = 0.0;
  double startAngle = 0.0;
  double endAngle = 0.0;
  if (!arcGuideFromThreePoints(points, center, radius, startAngle, endAngle))
  {
    log(tr("log.geometryArcInvalid") + ": " + camera.id);
    return;
  }

  GeometryArcRuntimeConfig& config = activeGeometryArcConfig(camera.id);
  config.imageCenter = center;
  config.imageStart = cv::Point2d(points[0].x(), points[0].y());
  config.imageEnd = cv::Point2d(points[1].x(), points[1].y());
  config.radius = radius;
  config.startAngleRadians = startAngle;
  config.endAngleRadians = endAngle;
  config.hasImageArc = true;

  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.hasArc = true;
  }

  largeImage()->setThreePointArcDrawingEnabled(false);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Arc;
  saveGeometryArcsRecipe(camera);
  showConfiguredGeometryArcs(camera);
  testGeometryArc(camera);
}

void MainWindowGeometryModule::showConfiguredGeometryArcs(const CameraConfig& camera)
{
  GeometryArcRuntimeConfig& arc = activeGeometryArcConfig(camera.id);
  largeImage()->clearCircles();
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const bool usePartArc = pose.valid && arc.hasArc;
  const cv::Point2d center = usePartArc ? partToImage(pose, arc.partCenter) : arc.imageCenter;
  const cv::Point2d start = usePartArc ? partToImage(pose, arc.partStart) : arc.imageStart;
  const cv::Point2d end = usePartArc ? partToImage(pose, arc.partEnd) : arc.imageEnd;
  const double radius = std::hypot(start.x - center.x, start.y - center.y);
  const double startAngle = normalizedAngle(std::atan2(start.y - center.y, start.x - center.x));
  const double endAngle = normalizedAngle(std::atan2(end.y - center.y, end.x - center.x));

  GeometryOverlay overlay;
  appendCurrentPartPoseOverlay(camera, overlay);
  if ((usePartArc || arc.hasImageArc) && arc.radius > arc.innerBand)
  {
    appendArcPolyline(overlay, center, std::max(1.0, radius - arc.innerBand), startAngle, endAngle, QColor(0, 210, 255, 90), 2);
    appendArcPolyline(overlay, center, radius, startAngle, endAngle, QColor("#ff4fd8"), 5);
    appendArcPolyline(overlay, center, radius + arc.outerBand, startAngle, endAngle, QColor(0, 210, 255, 90), 2);
    overlay.lines.append({QPointF(start.x, start.y), QPointF(end.x, end.y), QColor("#808a93"), 1});
  }
  largeImage()->setGeometryOverlay(overlay);
}

void MainWindowGeometryModule::testGeometryArc(const CameraConfig& camera)
{
  if (camera.id != selectedCameraId())
  {
    return;
  }

  GeometryArcRuntimeConfig& arcConfig = activeGeometryArcConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  if (pose.valid && !arcConfig.hasArc && arcConfig.hasImageArc)
  {
    arcConfig.partCenter = imageToPart(pose, arcConfig.imageCenter);
    arcConfig.partStart = imageToPart(pose, arcConfig.imageStart);
    arcConfig.partEnd = imageToPart(pose, arcConfig.imageEnd);
    arcConfig.hasArc = true;
    saveGeometryArcsRecipe(camera);
  }

  const bool usePartArc = pose.valid && arcConfig.hasArc;
  if (!usePartArc && !arcConfig.hasImageArc)
  {
    showConfiguredGeometryArcs(camera);
    log(tr("log.geometryArcMissing") + ": " + camera.id);
    return;
  }

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  const cv::Point2d guideCenter = usePartArc ? partToImage(pose, arcConfig.partCenter) : arcConfig.imageCenter;
  const cv::Point2d guideStart = usePartArc ? partToImage(pose, arcConfig.partStart) : arcConfig.imageStart;
  const cv::Point2d guideEnd = usePartArc ? partToImage(pose, arcConfig.partEnd) : arcConfig.imageEnd;
  const double guideRadius = std::hypot(guideStart.x - guideCenter.x, guideStart.y - guideCenter.y);

  EdgeCircleDetectorConfig config;
  config.id = arcConfig.id;
  config.label = arcConfig.id;
  config.guideCenter = guideCenter;
  config.guideRadius = guideRadius;
  config.innerBand = arcConfig.innerBand;
  config.outerBand = arcConfig.outerBand;
  config.edgeSensitivity = arcConfig.edgeSensitivity;
  config.edgeCleanupDerivative = arcConfig.edgeCleanupDerivative;
  config.edgeStatisticalFilter = arcConfig.edgeStatisticalFilter;
  config.useSubpixel = MainWindowCameraProfile::isBwDimensional(camera, MainWindowModuleBase::config()) && arcConfig.useSubpixel;
  config.scanDirection = arcConfig.scanDirection;
  config.transition = arcConfig.transition;
  config.pickMode = arcConfig.pickMode;
  config.useArc = true;
  config.startAngleRadians = normalizedAngle(std::atan2(guideStart.y - guideCenter.y, guideStart.x - guideCenter.x));
  config.endAngleRadians = normalizedAngle(std::atan2(guideEnd.y - guideCenter.y, guideEnd.x - guideCenter.x));

  log(QString("geometry arc detect: %1 id=%2 center=%3,%4 r=%5 band=%6/%7 sensitivity=%8 cleanup=%9 stat=%10 scan=%11 transition=%12 pick=%13")
        .arg(camera.id)
        .arg(arcConfig.id)
        .arg(guideCenter.x, 0, 'f', 1)
        .arg(guideCenter.y, 0, 'f', 1)
        .arg(guideRadius, 0, 'f', 1)
        .arg(arcConfig.innerBand)
        .arg(arcConfig.outerBand)
        .arg(arcConfig.edgeSensitivity)
        .arg(arcConfig.edgeCleanupDerivative)
        .arg(arcConfig.edgeStatisticalFilter)
        .arg(arcConfig.scanDirection == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive")
        .arg(arcConfig.transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark")
        .arg(arcConfig.pickMode == EdgeLinePickMode::Last ? "last" : (arcConfig.pickMode == EdgeLinePickMode::Best ? "best" : "first")));

  EdgeCircleDetector detector;
  const EdgeCircleDetectorResult result = detector.detect(input, config);
  if (!result.processed || result.diagnosticImage.empty())
  {
    log(result.message.isEmpty() ? tr("log.geometryArcFailed") + ": " + camera.id : result.message);
    return;
  }

  selectedPreview() = context().imaging->matToPixmap(result.diagnosticImage);
  largeImage()->setImage(selectedPreview());
  showConfiguredGeometryArcs(camera);

  if (!result.found)
  {
    log(result.message.isEmpty() ? tr("log.geometryArcNotFound") + ": " + camera.id : result.message);
    return;
  }

  GeometrySet& geometries = cameraRuntime()[camera.id].geometries();
  for (int i = geometries.arcs.size() - 1; i >= 0; --i)
  {
    if (geometries.arcs[i].meta.id == arcConfig.id)
    {
      geometries.arcs.removeAt(i);
    }
  }
  geometries.arcs.append(result.arc);
  GeometryOverlay arcOverlay;
  appendArcPolyline(arcOverlay, result.arc.center, result.arc.radius, result.arc.startAngleRadians, result.arc.endAngleRadians, QColor("#ff4fd8"), 7);
  appendCurrentPartPoseOverlay(camera, arcOverlay);
  largeImage()->setGeometryOverlay(arcOverlay);
  log(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(tr("log.geometryArcFound"))
              .arg(camera.id)
              .arg(result.arc.center.x, 0, 'f', 1)
              .arg(result.arc.center.y, 0, 'f', 1)
              .arg(result.arc.radius, 0, 'f', 1));
}
