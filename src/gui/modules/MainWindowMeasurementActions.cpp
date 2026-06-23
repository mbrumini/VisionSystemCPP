#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/modules/ConstructedGeometryCircleSource.h"
#include "gui/modules/ConstructedGeometryLineSource.h"
#include "gui/modules/ConstructedGeometryPointSource.h"
#include "measurement/MeasurementGeometryMath.h"
#include "runtime/CameraRuntime.h"

#include <cmath>

void MainWindowMeasurementModule::createPointPointDistance(const CameraConfig& camera,
                                                           const QString& pointAId,
                                                           const QString& pointBId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const PointGeometry* pointA = findConstructedGeometryPointSource(set, pointAId);
  const PointGeometry* pointB = findConstructedGeometryPointSource(set, pointBId);
  if (!pointA || !pointB)
  {
    log(QString("%1: %2").arg(tr("log.measurementPointPointMissing"), camera.id));
    return;
  }
  if (pointA->meta.id == pointB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementSameSourceSelected"), camera.id));
    return;
  }

  double distancePixels = 0.0;
  if (!MeasurementGeometryMath::pointPointDistance(*pointA, *pointB, distancePixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementPointPointInvalid"), camera.id));
    return;
  }

  if (!saveMeasurementRecipeAction(camera, "point_point_distance", pointA->meta.id, pointB->meta.id, distancePixels))
  {
    return;
  }

  finalizeMeasurementCreate(
    camera,
    QString("%1: %2 %3/%4 px=%5")
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
  if (!MeasurementGeometryMath::pointLineDistance(*point, *line, distancePixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementPointLineInvalid"), camera.id));
    return;
  }

  if (!saveMeasurementRecipeAction(camera, "point_line_distance", point->meta.id, line->meta.id, distancePixels))
  {
    return;
  }

  finalizeMeasurementCreate(
    camera,
    QString("%1: %2 %3/%4 px=%5")
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
  if (!lineA || !lineB)
  {
    log(QString("%1: %2").arg(tr("log.measurementLineLineMissing"), camera.id));
    return;
  }
  if (lineA->meta.id == lineB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementSameSourceSelected"), camera.id));
    return;
  }

  double distancePixels = 0.0;
  if (!MeasurementGeometryMath::parallelLineDistance(*lineA, *lineB, distancePixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementLineLineInvalid"), camera.id));
    return;
  }

  if (!saveMeasurementRecipeAction(camera, "line_line_distance", lineA->meta.id, lineB->meta.id, distancePixels))
  {
    return;
  }

  finalizeMeasurementCreate(
    camera,
    QString("%1: %2 %3/%4 px=%5")
      .arg(tr("log.measurementLineLineCreated"))
      .arg(camera.id)
      .arg(lineA->meta.id)
      .arg(lineB->meta.id)
      .arg(distancePixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createCircleDiameter(const CameraConfig& camera, const QString& circleId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  CircleGeometry circle;
  if (!constructedGeometryCircleSourceValue(set, circleId, circle))
  {
    log(QString("%1: %2").arg(tr("log.measurementCircleDiameterMissing"), camera.id));
    return;
  }

  double diameterPixels = 0.0;
  if (!MeasurementGeometryMath::circleDiameterPixels(circle, diameterPixels))
  {
    log(QString("%1: %2").arg(tr("log.measurementCircleDiameterInvalid"), camera.id));
    return;
  }

  if (!saveMeasurementRecipeAction(camera, "circle_diameter", circle.meta.id, {}, diameterPixels))
  {
    return;
  }

  finalizeMeasurementCreate(
    camera,
    QString("%1: %2 %3 px=%4")
      .arg(tr("log.measurementCircleDiameterCreated"))
      .arg(camera.id)
      .arg(circle.meta.id)
      .arg(diameterPixels, 0, 'f', 3));
}

void MainWindowMeasurementModule::createLineLineAngle(const CameraConfig& camera,
                                                      const QString& lineAId,
                                                      const QString& lineBId)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const LineGeometry* lineA = findConstructedGeometryLineSource(set, lineAId);
  const LineGeometry* lineB = findConstructedGeometryLineSource(set, lineBId);
  if (!lineA || !lineB)
  {
    log(QString("%1: %2").arg(tr("log.measurementLineAngleMissing"), camera.id));
    return;
  }
  if (lineA->meta.id == lineB->meta.id)
  {
    log(QString("%1: %2").arg(tr("log.measurementSameSourceSelected"), camera.id));
    return;
  }

  double angleDegrees = 0.0;
  if (!MeasurementGeometryMath::lineLineAngleDegrees(*lineA, *lineB, angleDegrees))
  {
    log(QString("%1: %2").arg(tr("log.measurementLineAngleInvalid"), camera.id));
    return;
  }

  if (!saveMeasurementRecipeAction(camera, "line_line_angle", lineA->meta.id, lineB->meta.id, angleDegrees))
  {
    return;
  }

  finalizeMeasurementCreate(
    camera,
    QString("%1: %2 %3/%4 deg=%5")
      .arg(tr("log.measurementLineAngleCreated"))
      .arg(camera.id)
      .arg(lineA->meta.id)
      .arg(lineB->meta.id)
      .arg(angleDegrees, 0, 'f', 3));
}
