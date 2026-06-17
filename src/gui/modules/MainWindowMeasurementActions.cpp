#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "measurement/MeasurementGeometryMath.h"
#include "runtime/CameraRuntime.h"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace
{
QPointF toPointF(const cv::Point2d& point)
{
  return QPointF(point.x, point.y);
}

cv::Point2d normalizedDirection(const LineGeometry& line)
{
  const cv::Point2d direction = line.end - line.start;
  const double length = std::sqrt(direction.dot(direction));
  if (length < 0.000001)
  {
    return {};
  }
  return direction * (1.0 / length);
}

bool lineIntersection(const LineGeometry& lineA, const LineGeometry& lineB, cv::Point2d& intersection)
{
  const cv::Point2d r = lineA.end - lineA.start;
  const cv::Point2d s = lineB.end - lineB.start;
  const double denominator = r.x * s.y - r.y * s.x;
  if (std::abs(denominator) < 0.000001)
  {
    return false;
  }

  const cv::Point2d delta = lineB.start - lineA.start;
  const double t = (delta.x * s.y - delta.y * s.x) / denominator;
  intersection = lineA.start + r * t;
  return true;
}

void appendMeasurement(GeometrySet& set,
                       const QString& id,
                       const QString& type,
                       const QString& sourceAId,
                       const QString& sourceBId,
                       double value)
{
  MeasurementResult result;
  result.id = id;
  result.type = type;
  result.sourceAId = sourceAId;
  result.sourceBId = sourceBId;
  result.valuePixels = value;
  result.valid = true;
  set.measurements.append(result);
}

QString measurementKey(const QString& type, const QString& sourceAId, const QString& sourceBId)
{
  return QString("%1|%2|%3").arg(type, sourceAId, sourceBId);
}

GeometryOverlayDimension measurementDimension(const QPointF& start,
                                              const QPointF& end,
                                              const QString& label,
                                              const QString& type,
                                              const QString& sourceAId,
                                              const QString& sourceBId)
{
  GeometryOverlayDimension dimension;
  dimension.imageStart = start;
  dimension.imageEnd = end;
  dimension.label = label;
  dimension.color = QColor("#ff8a00");
  dimension.width = 3;
  dimension.id = measurementKey(type, sourceAId, sourceBId);
  return dimension;
}
}

void MainWindowMeasurementModule::createPointPointDistance(const CameraConfig& camera,
                                                           const QString& pointAId,
                                                           const QString& pointBId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* pointA = findConstructedGeometryPointSource(set, pointAId);
  const PointGeometry* pointB = findConstructedGeometryPointSource(set, pointBId);
  if (!pointA || !pointB || pointA->meta.id == pointB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementPointPointMissing"), camera.id));
    return;
  }

  double distancePixels = 0.0;
  if (!MeasurementGeometryMath::pointPointDistance(*pointA, *pointB, distancePixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementPointPointInvalid"), camera.id));
    return;
  }

  appendMeasurement(set,
                    QString("%1_dist_%2").arg(pointA->meta.id, pointB->meta.id),
                    "point_point_distance",
                    pointA->meta.id,
                    pointB->meta.id,
                    distancePixels);
  saveMeasurementRecipeAction(camera, "point_point_distance", pointA->meta.id, pointB->meta.id, distancePixels);

  GeometryOverlay overlay;
  overlay.dimensions.append(measurementDimension(
    toPointF(pointA->point),
    toPointF(pointB->point),
    QString("%1 px").arg(distancePixels, 0, 'f', 3),
    "point_point_distance",
    pointA->meta.id,
    pointB->meta.id));
  overlay.points.append({toPointF(pointA->point), "A", QColor("#35c46a")});
  overlay.points.append({toPointF(pointB->point), "B", QColor("#35c46a")});
  largeImage()->setGeometryOverlay(overlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }

  log(QString("%1: %2 %3/%4 px=%5")
        .arg(tr("log.measurementPointPointCreated"))
        .arg(camera.id)
        .arg(pointA->meta.id)
        .arg(pointB->meta.id)
        .arg(distancePixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createPointLineDistance(const CameraConfig& camera,
                                                          const QString& pointId,
                                                          const QString& lineId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* point = findConstructedGeometryPointSource(set, pointId);
  const LineGeometry* line = findConstructedGeometryLineSource(set, lineId);
  if (!point || !line)
  {
    log(QString("%1: %2").arg(tr("log.measurementPointLineMissing"), camera.id));
    return;
  }

  double distancePixels = 0.0;
  PointGeometry projectedPoint;
  if (!MeasurementGeometryMath::pointLineDistance(*point, *line, distancePixels, &projectedPoint))
  {
    log(QString("%1: %2").arg(tr("log.measurementPointLineInvalid"), camera.id));
    return;
  }

  appendMeasurement(set,
                    QString("%1_dist_%2").arg(point->meta.id, line->meta.id),
                    "point_line_distance",
                    point->meta.id,
                    line->meta.id,
                    distancePixels);
  saveMeasurementRecipeAction(camera, "point_line_distance", point->meta.id, line->meta.id, distancePixels);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(line->start), toPointF(line->end), QColor("#35c46a"), 4});
  overlay.dimensions.append(measurementDimension(
    toPointF(point->point),
    toPointF(projectedPoint.point),
    QString("%1 px").arg(distancePixels, 0, 'f', 3),
    "point_line_distance",
    point->meta.id,
    line->meta.id));
  overlay.points.append({toPointF(point->point), "P", QColor("#35c46a")});
  overlay.points.append({toPointF(projectedPoint.point), "H", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }

  log(QString("%1: %2 %3/%4 px=%5")
        .arg(tr("log.measurementPointLineCreated"))
        .arg(camera.id)
        .arg(point->meta.id)
        .arg(line->meta.id)
        .arg(distancePixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createLineLineDistance(const CameraConfig& camera,
                                                         const QString& lineAId,
                                                         const QString& lineBId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* lineA = findConstructedGeometryLineSource(set, lineAId);
  const LineGeometry* lineB = findConstructedGeometryLineSource(set, lineBId);
  if (!lineA || !lineB || lineA->meta.id == lineB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementLineLineMissing"), camera.id));
    return;
  }

  double distancePixels = 0.0;
  PointGeometry pointOnA;
  PointGeometry pointOnB;
  if (!MeasurementGeometryMath::parallelLineDistance(*lineA, *lineB, distancePixels, &pointOnA, &pointOnB))
  {
    log(QString("%1: %2").arg(tr("log.measurementLineLineInvalid"), camera.id));
    return;
  }

  appendMeasurement(set,
                    QString("%1_dist_%2").arg(lineA->meta.id, lineB->meta.id),
                    "line_line_distance",
                    lineA->meta.id,
                    lineB->meta.id,
                    distancePixels);
  saveMeasurementRecipeAction(camera, "line_line_distance", lineA->meta.id, lineB->meta.id, distancePixels);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(lineA->start), toPointF(lineA->end), QColor("#35c46a"), 4});
  overlay.lines.append({toPointF(lineB->start), toPointF(lineB->end), QColor("#35c46a"), 4});
  overlay.dimensions.append(measurementDimension(
    toPointF(pointOnA.point),
    toPointF(pointOnB.point),
    QString("%1 px").arg(distancePixels, 0, 'f', 3),
    "line_line_distance",
    lineA->meta.id,
    lineB->meta.id));
  overlay.points.append({toPointF(pointOnA.point), "A", QColor("#ff8a00")});
  overlay.points.append({toPointF(pointOnB.point), "H", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }

  log(QString("%1: %2 %3/%4 px=%5")
        .arg(tr("log.measurementLineLineCreated"))
        .arg(camera.id)
        .arg(lineA->meta.id)
        .arg(lineB->meta.id)
        .arg(distancePixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createCircleDiameter(const CameraConfig& camera, const QString& circleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const CircleGeometry* circle = findConstructedGeometryCircleSource(set.circles, circleId);
  CircleGeometry arcCircle;
  if (!circle)
  {
    for (const ArcGeometry& arc : set.arcs)
    {
      if (arc.meta.id == circleId)
      {
        arcCircle.meta = arc.meta;
        arcCircle.center = arc.center;
        arcCircle.radius = arc.radius;
        arcCircle.meanError = arc.meanError;
        circle = &arcCircle;
        break;
      }
    }
  }
  if (!circle)
  {
    log(QString("%1: %2").arg(tr("log.measurementCircleDiameterMissing"), camera.id));
    return;
  }

  double diameterPixels = 0.0;
  if (!MeasurementGeometryMath::circleDiameterPixels(*circle, diameterPixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementCircleDiameterInvalid"), camera.id));
    return;
  }

  appendMeasurement(set,
                    QString("%1_diameter").arg(circle->meta.id),
                    "circle_diameter",
                    circle->meta.id,
                    {},
                    diameterPixels);
  saveMeasurementRecipeAction(camera, "circle_diameter", circle->meta.id, {}, diameterPixels);

  const cv::Point2d left(circle->center.x - circle->radius, circle->center.y);
  const cv::Point2d right(circle->center.x + circle->radius, circle->center.y);
  const cv::Point2d top(circle->center.x, circle->center.y - circle->radius);
  const cv::Point2d bottom(circle->center.x, circle->center.y + circle->radius);

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(top), toPointF(bottom), QColor("#35c46a"), 2});
  overlay.dimensions.append(measurementDimension(
    toPointF(left),
    toPointF(right),
    QString("%1 px").arg(diameterPixels, 0, 'f', 3),
    "circle_diameter",
    circle->meta.id,
    {}));
  overlay.points.append({toPointF(circle->center), "C", QColor("#35c46a")});
  largeImage()->setGeometryOverlay(overlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }

  log(QString("%1: %2 %3 px=%4")
        .arg(tr("log.measurementCircleDiameterCreated"))
        .arg(camera.id)
        .arg(circle->meta.id)
        .arg(diameterPixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createLineLineAngle(const CameraConfig& camera,
                                                      const QString& lineAId,
                                                      const QString& lineBId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* lineA = findConstructedGeometryLineSource(set, lineAId);
  const LineGeometry* lineB = findConstructedGeometryLineSource(set, lineBId);
  if (!lineA || !lineB || lineA->meta.id == lineB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementLineAngleMissing"), camera.id));
    return;
  }

  double angleDegrees = 0.0;
  if (!MeasurementGeometryMath::lineLineAngleDegrees(*lineA, *lineB, angleDegrees))
  {
    log(QString("%1: %2").arg(tr("log.measurementLineAngleInvalid"), camera.id));
    return;
  }

  appendMeasurement(set,
                    QString("%1_angle_%2").arg(lineA->meta.id, lineB->meta.id),
                    "line_line_angle",
                    lineA->meta.id,
                    lineB->meta.id,
                    angleDegrees);
  saveMeasurementRecipeAction(camera, "line_line_angle", lineA->meta.id, lineB->meta.id, angleDegrees);

  cv::Point2d center;
  if (!lineIntersection(*lineA, *lineB, center))
  {
    center = lineA->start;
  }

  cv::Point2d directionA = normalizedDirection(*lineA);
  cv::Point2d directionB = normalizedDirection(*lineB);
  if (directionA.dot(directionB) < 0.0)
  {
    directionB *= -1.0;
  }
  const double armLength = std::min(90.0, std::max(35.0, std::min(lineLength(*lineA), lineLength(*lineB)) * 0.35));

  GeometryOverlay overlay;
  overlay.lines.append({toPointF(lineA->start), toPointF(lineA->end), QColor("#35c46a"), 4});
  overlay.lines.append({toPointF(lineB->start), toPointF(lineB->end), QColor("#35c46a"), 4});
  overlay.angles.append({
    toPointF(center),
    toPointF(center + directionA * armLength),
    toPointF(center + directionB * armLength),
    QString("%1 deg").arg(angleDegrees, 0, 'f', 3),
    QColor("#ff8a00"),
    3
  });
  overlay.points.append({toPointF(center), "V", QColor("#ff8a00")});
  largeImage()->setGeometryOverlay(overlay);
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }

  log(QString("%1: %2 %3/%4 deg=%5")
        .arg(tr("log.measurementLineAngleCreated"))
        .arg(camera.id)
        .arg(lineA->meta.id)
        .arg(lineB->meta.id)
        .arg(angleDegrees, 0, 'f', 3));
}
