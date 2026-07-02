#include "gui/modules/MainWindowGeometryModule.h"
#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/modules/MainWindowImagingModule.h"
#include "gui/modules/MainWindowSurfaceModule.h"
#include "gui/modules/MainWindowContext.h"

#include "config/RecipeJsonUtils.h"

#include "gui/geometry/ArcGuideMath.h"
#include "gui/geometry/GeometryGuideRuntime.h"
#include "gui/geometry/GeometryOverlayPrimitives.h"

#include "processing/geometry/EdgeCircleDetector.h"
#include "processing/geometry/EdgeCircleDetectorExperimental.h"

#include <QColor>
#include <QPointF>

#include <algorithm>
#include <cmath>

namespace
{
using ArcGuideMath::arcMidAngle;
using ArcGuideMath::normalizedAngle;
using ArcGuideMath::pointOnArc;
using ArcGuideMath::resolveArcGuide;
using ArcGuideMath::syncArcPartAngles;

bool containsNearbyEdgePoint(const std::vector<cv::Point2d>& points, const cv::Point2d& target)
{
  constexpr double kNearSquared = 0.25;
  for (const cv::Point2d& point : points)
  {
    const cv::Point2d delta = point - target;
    if ((delta.x * delta.x + delta.y * delta.y) <= kNearSquared)
    {
      return true;
    }
  }
  return false;
}

void appendEdgeFilterOverlay(GeometryOverlay& overlay,
                             const EdgeCircleDetectorResult& result,
                             const cv::Point2d& labelPoint)
{
  for (const cv::Point2d& point : result.sectorEdgePoints)
  {
    if (!containsNearbyEdgePoint(result.statisticalEdgePoints, point))
    {
      overlay.points.append({QPointF(point.x, point.y), QString(), QColor("#ff3b30"), 4.0, false});
    }
  }

  for (const cv::Point2d& point : result.statisticalEdgePoints)
  {
    if (!containsNearbyEdgePoint(result.edgePoints, point))
    {
      overlay.points.append({QPointF(point.x, point.y), QString(), QColor("#ff8a00"), 3.5, false});
    }
  }

  for (const cv::Point2d& point : result.edgePoints)
  {
    overlay.points.append({QPointF(point.x, point.y), QString(), QColor("#f5d547"), 3.0, false});
  }

  overlay.points.append({
    QPointF(labelPoint.x, labelPoint.y),
    QString("stat %1/%2  active %3")
      .arg(result.statisticalEdgePoints.size())
      .arg(result.sectorEdgePoints.size())
      .arg(result.edgePoints.size()),
    QColor("#ffffff"),
    2.0,
    false
  });
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

void syncArcStoredPoints(GeometryArcRuntimeConfig& arc)
{
  arc.imageStart = pointOnArc(arc.imageCenter, arc.radius, arc.startAngleRadians);
  arc.imageEnd = pointOnArc(arc.imageCenter, arc.radius, arc.endAngleRadians);
  if (arc.hasImageThrough)
  {
    arc.imageThrough = pointOnArc(arc.imageCenter, arc.radius, arcMidAngle(arc.startAngleRadians, arc.endAngleRadians));
  }
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

void MainWindowGeometryModule::addGeometryArc(const CameraConfig& camera)
{
  QVector<GeometryArcRuntimeConfig>& arcs = m_arcConfigs[camera.id];
  GeometryArcRuntimeConfig arc;
  QStringList existingIds;
  existingIds.reserve(arcs.size());
  for (const GeometryArcRuntimeConfig& existing : arcs)
  {
    existingIds.append(existing.id);
  }
  arc.id = RecipeJsonUtils::nextPrefixedId("arc", existingIds);
  arcs.append(arc);
  m_activeArcIndexes[camera.id] = arcs.size() - 1;
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
  config.imageThrough = cv::Point2d(points[2].x(), points[2].y());
  config.hasImageThrough = true;
  config.radius = radius;
  config.startAngleRadians = startAngle;
  config.endAngleRadians = endAngle;
  config.hasImageArc = true;
  syncArcStoredPoints(config);

  PartPose pose = cameraRuntime()[camera.id].currentPose();
  if (!pose.valid && context().surface && context().surface->restoreSurfaceModelPoseFromSample(camera))
  {
    pose = cameraRuntime()[camera.id].currentPose();
  }
  if (pose.valid)
  {
    config.partCenter = imageToPart(pose, config.imageCenter);
    config.partStart = imageToPart(pose, config.imageStart);
    config.partEnd = imageToPart(pose, config.imageEnd);
    config.partThrough = imageToPart(pose, config.imageThrough);
    config.hasArc = true;
    syncArcPartAngles(config);
  }
  else
  {
    config.hasArc = false;
  }

  largeImage()->setThreePointArcDrawingEnabled(false);
  largeImage()->setGeometryOverlayPointEditingEnabled(true);
  *context().activeDrawingRecipe = MainWindowActiveDrawingRecipe::Geometry;
  m_drawingTarget = DrawingTarget::Arc;
  showConfiguredGeometryArcs(camera);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

void MainWindowGeometryModule::handleGeometryArcHandleMoved(const CameraConfig& camera, int pointIndex, const QPointF& imagePoint)
{
  if (camera.id != selectedCameraId() || pointIndex < 0 || pointIndex > 3)
  {
    return;
  }

  GeometryArcRuntimeConfig& arc = activeGeometryArcConfig(camera.id);
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  const bool usePartArc = pose.valid && arc.hasArc && !arc.anchorInImageSpace;
  if (!usePartArc && !arc.hasImageArc)
  {
    return;
  }

  cv::Point2d center = usePartArc ? partToImage(pose, arc.partCenter) : arc.imageCenter;
  cv::Point2d start = usePartArc ? partToImage(pose, arc.partStart) : arc.imageStart;
  cv::Point2d end = usePartArc ? partToImage(pose, arc.partEnd) : arc.imageEnd;
  const cv::Point2d movedPoint(imagePoint.x(), imagePoint.y());

  if (pointIndex == 0)
  {
    const cv::Point2d delta = movedPoint - center;
    center = movedPoint;
    start += delta;
    end += delta;
  }
  else if (pointIndex == 1)
  {
    const double radius = std::max(1.0, std::hypot(movedPoint.x - center.x, movedPoint.y - center.y));
    const double endAngle = std::atan2(end.y - center.y, end.x - center.x);
    start = movedPoint;
    end = pointOnArc(center, radius, endAngle);
  }
  else if (pointIndex == 2)
  {
    const double radius = std::max(1.0, std::hypot(movedPoint.x - center.x, movedPoint.y - center.y));
    const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
    const double endAngle = std::atan2(end.y - center.y, end.x - center.x);
    start = pointOnArc(center, radius, startAngle);
    end = pointOnArc(center, radius, endAngle);
  }
  else
  {
    const double radius = std::max(1.0, std::hypot(movedPoint.x - center.x, movedPoint.y - center.y));
    const double startAngle = std::atan2(start.y - center.y, start.x - center.x);
    end = movedPoint;
    start = pointOnArc(center, radius, startAngle);
  }

  arc.imageCenter = center;
  arc.imageStart = start;
  arc.imageEnd = end;
  arc.radius = std::max(1.0, std::hypot(start.x - center.x, start.y - center.y));
  arc.startAngleRadians = normalizedAngle(std::atan2(start.y - center.y, start.x - center.x));
  arc.endAngleRadians = normalizedAngle(std::atan2(end.y - center.y, end.x - center.x));
  arc.hasImageArc = true;
  syncArcStoredPoints(arc);

  if (pose.valid)
  {
    arc.partCenter = imageToPart(pose, arc.imageCenter);
    arc.partStart = imageToPart(pose, arc.imageStart);
    arc.partEnd = imageToPart(pose, arc.imageEnd);
    if (arc.hasImageThrough)
    {
      arc.partThrough = imageToPart(pose, arc.imageThrough);
    }
    arc.hasArc = true;
    syncArcPartAngles(arc);
  }
  else
  {
    arc.hasArc = false;
  }

  showConfiguredGeometryArcs(camera);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
}

void MainWindowGeometryModule::showConfiguredGeometryArcs(const CameraConfig& camera)
{
  GeometryArcRuntimeConfig& arc = activeGeometryArcConfig(camera.id);
  largeImage()->clearCircles();
  const PartPose& pose = cameraRuntime()[camera.id].currentPose();
  cv::Size imageSize;
  const cv::Mat& frame = cameraRuntime()[camera.id].currentFrame();
  if (!frame.empty())
  {
    imageSize = frame.size();
  }
  GeometryOverlay overlay;
  QVector<GeometryArcRuntimeConfig>& arcList = m_arcConfigs[camera.id];
  const int activeIndex = qBound(0, m_activeArcIndexes.value(camera.id, 0), arcList.size() - 1);
  const QSize referenceSize = guideReferenceSize(camera.id);
  for (int i = 0; i < arcList.size(); ++i)
  {
    GeometryArcRuntimeConfig& item = arcList[i];
    QVector<GeometryLineRuntimeConfig> lines;
    QVector<GeometryPointRuntimeConfig> points;
    QVector<GeometryCircleRuntimeConfig> circles;
    QVector<GeometryArcRuntimeConfig> arcs = {item};
    GeometryGuideRuntime::syncPartGuidesFromImage(pose, lines, points, circles, arcs);
    item = arcs.first();

    ResolvedArcGuide guide;
    if (!resolveArcGuide(item, pose, guide, referenceSize, imageSize))
    {
      continue;
    }

    const bool active = i == activeIndex;
    const double innerRadius = std::max(1.0, guide.radius - item.innerBand);
    const double outerRadius = guide.radius + item.outerBand;
    appendArcPolyline(overlay, guide.center, innerRadius, guide.startAngle, guide.endAngle, QColor(0, 210, 255, active ? 90 : 35), active ? 2 : 1);
    appendArcPolyline(overlay, guide.center, guide.radius, guide.startAngle, guide.endAngle, active ? QColor("#ff4fd8") : QColor(170, 205, 220, 170), active ? 5 : 2);
    appendArcPolyline(overlay, guide.center, outerRadius, guide.startAngle, guide.endAngle, QColor(0, 210, 255, active ? 90 : 35), active ? 2 : 1);
    if (!active)
    {
      continue;
    }

    appendGeometryArcSearchBandGuides(
      overlay,
      guide.center,
      innerRadius,
      outerRadius,
      guide.startAngle,
      guide.endAngle,
      item.scanDirection != EdgeLineScanDirection::NormalNegative,
      QColor(0, 210, 255, 150),
      2);
    overlay.lines.append({QPointF(guide.start.x, guide.start.y), QPointF(guide.end.x, guide.end.y), QColor("#808a93"), 1});
    const cv::Point2d radiusHandle = pointOnArc(guide.center, guide.radius, arcMidAngle(guide.startAngle, guide.endAngle));
    overlay.points.append({QPointF(guide.center.x, guide.center.y), "C", QColor("#ff4fd8")});
    overlay.points.append({QPointF(guide.start.x, guide.start.y), "1", QColor("#ff4fd8")});
    overlay.points.append({QPointF(radiusHandle.x, radiusHandle.y), "R", QColor("#ff4fd8")});
    overlay.points.append({QPointF(guide.end.x, guide.end.y), "2", QColor("#ff4fd8")});
  }
  appendCurrentPartPoseOverlay(camera, overlay);
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
  if (!pose.valid && context().surface)
  {
    context().surface->localizePoseOnSample(camera);
  }
  const PartPose& resolvedPose = cameraRuntime()[camera.id].currentPose();
  QVector<GeometryLineRuntimeConfig> lines;
  QVector<GeometryPointRuntimeConfig> points;
  QVector<GeometryCircleRuntimeConfig> circles;
  QVector<GeometryArcRuntimeConfig> arcs = {arcConfig};
  GeometryGuideRuntime::forceSyncPartGuidesFromImage(resolvedPose, lines, points, circles, arcs);
  arcConfig = arcs.first();

  QString imageError;
  const cv::Mat input = context().imaging->currentInputImage(camera, &imageError);
  if (input.empty())
  {
    log(imageError);
    return;
  }

  ResolvedArcGuide guide;
  if (!resolveArcGuide(arcConfig, resolvedPose, guide, guideReferenceSize(camera.id), input.size()))
  {
    showConfiguredGeometryArcs(camera);
    log(tr("log.geometryArcMissing") + ": " + camera.id);
    return;
  }

  const cv::Point2d guideCenter = guide.center;
  const cv::Point2d guideStart = guide.start;
  const cv::Point2d guideEnd = guide.end;
  const double guideRadius = guide.radius;

  EdgeCircleDetectorConfig config;
  config.id = arcConfig.id;
  config.label = arcConfig.alias.trimmed().isEmpty() ? arcConfig.id : arcConfig.alias.trimmed();
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
  config.startAngleRadians = guide.startAngle;
  config.endAngleRadians = guide.endAngle;

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

  const EdgeCircleDetectorResult result = detectEdgeCircleWithSelectedDetector(input, config);
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
  appendArcPolyline(arcOverlay, guide.center, guide.radius, guide.startAngle, guide.endAngle, QColor("#ff4fd8"), 5);
  if (result.found)
  {
    appendArcPolyline(
      arcOverlay,
      result.arc.center,
      result.arc.radius,
      result.arc.startAngleRadians,
      result.arc.endAngleRadians,
      QColor("#00d2ff"),
      3);
  }
  const cv::Point2d guideRadiusHandle = pointOnArc(guide.center, guide.radius, arcMidAngle(guide.startAngle, guide.endAngle));
  appendEdgeFilterOverlay(arcOverlay, result, guideRadiusHandle);
  arcOverlay.points.append({QPointF(guide.center.x, guide.center.y), "C", QColor("#ff4fd8")});
  arcOverlay.points.append({QPointF(guide.start.x, guide.start.y), "1", QColor("#ff4fd8")});
  arcOverlay.points.append({QPointF(guideRadiusHandle.x, guideRadiusHandle.y), "R", QColor("#ff4fd8")});
  arcOverlay.points.append({QPointF(guide.end.x, guide.end.y), "2", QColor("#ff4fd8")});
  appendCurrentPartPoseOverlay(camera, arcOverlay);
  largeImage()->setGeometryOverlay(arcOverlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
  log(QString("%1: %2 cx=%3 cy=%4 r=%5")
              .arg(tr("log.geometryArcFound"))
              .arg(camera.id)
              .arg(result.arc.center.x, 0, 'f', 1)
              .arg(result.arc.center.y, 0, 'f', 1)
              .arg(result.arc.radius, 0, 'f', 1));
}
