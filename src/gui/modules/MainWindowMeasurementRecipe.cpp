#include "gui/modules/MainWindowMeasurementModule.h"

#include "gui/geometry/GeometryOverlay.h"
#include "gui/modules/MainWindowGeometryModule.h"
#include "measurement/MeasurementGeometryMath.h"
#include "processing/GeometryMeasurementPipeline.h"
#include "runtime/CameraRuntime.h"

#include <QColor>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace
{
CameraMeasurementCalibration calibrationFromCamera(const CameraConfig& camera)
{
  CameraMeasurementCalibration calibration;
  calibration.enabled = camera.calibration.enabled;
  calibration.pixelSizeXMm = camera.calibration.pixelSizeXMm;
  calibration.pixelSizeYMm = camera.calibration.pixelSizeYMm;
  return calibration;
}

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

const LineGeometry* findLineByMetaId(const GeometrySet& set, const QString& id)
{
  for (const LineGeometry& line : set.lines)
  {
    if (line.meta.id == id)
    {
      return &line;
    }
  }
  for (const ConstructedLineGeometry& line : set.constructedLines)
  {
    if (line.line.meta.id == id)
    {
      return &line.line;
    }
  }
  return nullptr;
}

const PointGeometry* findPointByMetaId(const GeometrySet& set, const QString& id)
{
  for (const PointGeometry& point : set.points)
  {
    if (point.meta.id == id)
    {
      return &point;
    }
  }
  for (const ConstructedPointGeometry& point : set.constructedPoints)
  {
    if (point.point.meta.id == id)
    {
      return &point.point;
    }
  }
  static thread_local PointGeometry circleCenterPoint;
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == id)
    {
      circleCenterPoint.point = circle.center;
      circleCenterPoint.meta = circle.meta;
      circleCenterPoint.meta.valid = true;
      return &circleCenterPoint;
    }
  }
  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == id)
    {
      circleCenterPoint.point = arc.center;
      circleCenterPoint.meta = arc.meta;
      circleCenterPoint.meta.valid = true;
      return &circleCenterPoint;
    }
  }
  return nullptr;
}

const CircleGeometry* findCircleByMetaId(const GeometrySet& set, const QString& id)
{
  static thread_local CircleGeometry arcCircle;
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == id)
    {
      return &circle;
    }
  }
  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == id)
    {
      arcCircle.meta = arc.meta;
      arcCircle.center = arc.center;
      arcCircle.radius = arc.radius;
      arcCircle.meanError = arc.meanError;
      return &arcCircle;
    }
  }
  return nullptr;
}

void appendMeasurementResult(GeometrySet& set,
                             const CameraConfig& camera,
                             const MeasurementRecipeConfig& config,
                             const QString& sourceAId,
                             const QString& sourceBId,
                             double value)
{
  MeasurementResult result;
  result.id = config.id;
  result.alias = config.alias;
  result.type = config.type;
  result.sourceAId = sourceAId;
  result.sourceBId = sourceBId;
  result.valuePixels = value;
  result.valid = true;
  result.labelPoint = config.labelPoint;
  result.hasLabelPoint = config.hasLabelPoint;
  result.unit = config.unit;
  result.nominal = config.nominal;
  result.min = config.min;
  result.max = config.max;
  result.hasNominal = config.hasNominal;
  result.hasMin = config.hasMin;
  result.hasMax = config.hasMax;
  if (config.type == "line_line_angle")
  {
    result.valueReal = value;
    result.unit = "deg";
    result.hasRealValue = true;
  }
  else if (config.hasSampleScale && config.samplePixels > 0.000001)
  {
    result.valueReal = value * (config.sampleValue / config.samplePixels);
    result.unit = config.unit.isEmpty() || config.unit == "px" ? "mm" : config.unit;
    result.hasRealValue = true;
  }
  else if (camera.calibration.enabled &&
           camera.calibration.pixelSizeXMm > 0.000001 &&
           camera.calibration.pixelSizeYMm > 0.000001)
  {
    const double pixelSizeMm = (camera.calibration.pixelSizeXMm + camera.calibration.pixelSizeYMm) * 0.5;
    result.valueReal = value * pixelSizeMm;
    result.unit = "mm";
    result.hasRealValue = true;
  }
  else
  {
    result.unit = "px";
  }

  if (result.hasRealValue && (result.hasMin || result.hasMax))
  {
    const bool belowMin = result.hasMin && result.valueReal < result.min;
    const bool aboveMax = result.hasMax && result.valueReal > result.max;
    result.judgement = (belowMin || aboveMax) ? "NOK" : "OK";
  }
  set.measurements.append(result);
}

void appendFailedMeasurementResult(GeometrySet& set, const MeasurementRecipeConfig& config)
{
  MeasurementResult result;
  result.id = config.id;
  result.alias = config.alias;
  result.type = config.type;
  result.sourceAId = config.sourceAId;
  result.sourceBId = config.sourceBId;
  result.valid = false;
  result.judgement = "N/D";
  result.labelPoint = config.labelPoint;
  result.hasLabelPoint = config.hasLabelPoint;
  result.unit = config.unit.isEmpty() ? "px" : config.unit;
  if (config.type == "line_line_angle")
  {
    result.unit = "deg";
  }
  result.nominal = config.nominal;
  result.min = config.min;
  result.max = config.max;
  result.hasNominal = config.hasNominal;
  result.hasMin = config.hasMin;
  result.hasMax = config.hasMax;
  set.measurements.append(result);
}

QString measurementLabel(const MeasurementResult& measurement, const QString& unit)
{
  const QString name = measurement.alias.trimmed();
  const QString prefix = name.isEmpty() ? QString() : name + ": ";
  if (measurement.hasRealValue)
  {
    return prefix + QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  return prefix + QString("%1 %2").arg(measurement.valuePixels, 0, 'f', 3).arg(unit);
}

QString measurementKey(const MeasurementRecipeConfig& config)
{
  return QString("%1|%2|%3").arg(config.type, config.sourceAId, config.sourceBId);
}

QString measurementKey(const QString& type, const QString& sourceAId, const QString& sourceBId)
{
  return QString("%1|%2|%3").arg(type, sourceAId, sourceBId);
}

QColor measurementLabelColor(const MeasurementResult& measurement)
{
  if (measurement.judgement == "OK")
  {
    return QColor("#35c46a");
  }
  if (measurement.judgement == "NOK")
  {
    return QColor("#ff2f2f");
  }
  return QColor("#ff8a00");
}

GeometryOverlayDimension measurementDimension(const QPointF& start,
                                              const QPointF& end,
                                              const MeasurementResult& measurement,
                                              int width = 3)
{
  GeometryOverlayDimension dimension;
  dimension.imageStart = start;
  dimension.imageEnd = end;
  dimension.label = measurementLabel(measurement, "px");
  dimension.color = QColor("#ff8a00");
  dimension.labelColor = measurementLabelColor(measurement);
  dimension.width = width;
  dimension.id = measurementKey(measurement.type, measurement.sourceAId, measurement.sourceBId);
  dimension.labelPoint = measurement.labelPoint;
  dimension.hasLabelPoint = measurement.hasLabelPoint;
  return dimension;
}
}

bool MainWindowMeasurementModule::saveMeasurementRecipeAction(const CameraConfig& camera,
                                                              const QString& type,
                                                              const QString& sourceAId,
                                                              const QString& sourceBId,
                                                              double samplePixels)
{
  MeasurementRecipeConfig config;
  config.enabled = true;
  config.type = type;
  config.sourceAId = sourceAId;
  config.sourceBId = sourceBId;
  config.samplePixels = samplePixels;
  config.unit = type == "line_line_angle" ? "deg" : "px";

  QString error;
  if (!recipes().appendMeasurement(camera.id, config, &error))
  {
    log(QString("%1: %2").arg(tr("log.measurementRecipeSaveFailed"), error));
    return false;
  }
  return true;
}

void MainWindowMeasurementModule::finalizeMeasurementCreate(const CameraConfig& camera, const QString& successLog)
{
  rebuildMeasurementRecipe(camera);
  if (context().geometry && camera.id == selectedCameraId())
  {
    context().geometry->showRuntimeGeometryOverlay(camera);
  }
  if (context().updateMeasurementResults)
  {
    context().updateMeasurementResults();
  }
  log(successLog);
}

void MainWindowMeasurementModule::setMeasurementLabelPosition(const CameraConfig& camera,
                                                              const QString& key,
                                                              const QPointF& imagePoint)
{
  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  bool changed = false;
  for (MeasurementRecipeConfig& config : configs)
  {
    if (measurementKey(config) == key)
    {
      config.labelPoint = imagePoint;
      config.hasLabelPoint = true;
      changed = true;
      break;
    }
  }

  if (!changed)
  {
    return;
  }

  QString error;
  if (!recipes().saveMeasurements(camera.id, configs, &error))
  {
    log(QString("%1: %2").arg(tr("log.measurementRecipeSaveFailed"), error));
  }
}

void MainWindowMeasurementModule::saveMeasurementRealSettings(const CameraConfig& camera,
                                                              const MeasurementRecipeConfig& updatedConfig)
{
  QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  bool changed = false;
  for (MeasurementRecipeConfig& config : configs)
  {
    if (measurementKey(config) == measurementKey(updatedConfig))
    {
      config.alias = updatedConfig.alias.trimmed();
      config.unit = updatedConfig.unit;
      config.samplePixels = updatedConfig.samplePixels;
      config.sampleValue = updatedConfig.sampleValue;
      config.hasSampleScale = updatedConfig.hasSampleScale;
      config.nominal = updatedConfig.nominal;
      config.min = updatedConfig.min;
      config.max = updatedConfig.max;
      config.hasNominal = updatedConfig.hasNominal;
      config.hasMin = updatedConfig.hasMin;
      config.hasMax = updatedConfig.hasMax;
      changed = true;
      break;
    }
  }

  if (!changed)
  {
    return;
  }

  QString error;
  if (!recipes().saveMeasurements(camera.id, configs, &error))
  {
    log(QString("%1: %2").arg(tr("log.measurementRecipeSaveFailed"), error));
  }
}

void MainWindowMeasurementModule::rebuildMeasurementRecipe(const CameraConfig& camera)
{
  rebuildMeasurements(
    cameraRuntime()[camera.id].geometries(),
    recipes().loadMeasurements(camera.id),
    calibrationFromCamera(camera));
}

void MainWindowMeasurementModule::appendMeasurementOverlay(
  const CameraConfig& camera,
  GeometryOverlay& overlay,
  bool compact) const
{
  const GeometrySet& set = cameraRuntime()[camera.id].geometries();
  const int dimWidth = compact ? 2 : 3;
  const int angleWidth = compact ? 2 : 3;
  const double angleArmLength = compact ? 40.0 : 0.0;

  for (const MeasurementResult& measurement : set.measurements)
  {
    if (!measurement.valid)
    {
      continue;
    }

    if (measurement.type == "point_point_distance")
    {
      const PointGeometry* pointA = findPointByMetaId(set, measurement.sourceAId);
      const PointGeometry* pointB = findPointByMetaId(set, measurement.sourceBId);
      if (!pointA || !pointB)
      {
        continue;
      }

      overlay.dimensions.append(
        measurementDimension(toPointF(pointA->point), toPointF(pointB->point), measurement, dimWidth));
      if (!compact)
      {
        overlay.points.append({toPointF(pointA->point), "A", QColor("#35c46a")});
        overlay.points.append({toPointF(pointB->point), "B", QColor("#35c46a")});
      }
      continue;
    }

    if (measurement.type == "point_line_distance")
    {
      const PointGeometry* point = findPointByMetaId(set, measurement.sourceAId);
      const LineGeometry* line = findLineByMetaId(set, measurement.sourceBId);
      PointGeometry projectedPoint;
      double distancePixels = 0.0;
      if (!point || !line || !MeasurementGeometryMath::pointLineDistance(*point, *line, distancePixels, &projectedPoint))
      {
        continue;
      }

      overlay.dimensions.append(
        measurementDimension(toPointF(point->point), toPointF(projectedPoint.point), measurement, dimWidth));
      if (!compact)
      {
        overlay.points.append({toPointF(point->point), "P", QColor("#35c46a")});
        overlay.points.append({toPointF(projectedPoint.point), "H", QColor("#ff8a00")});
      }
      continue;
    }

    if (measurement.type == "line_line_distance")
    {
      const LineGeometry* lineA = findLineByMetaId(set, measurement.sourceAId);
      const LineGeometry* lineB = findLineByMetaId(set, measurement.sourceBId);
      PointGeometry pointOnA;
      PointGeometry pointOnB;
      double distancePixels = 0.0;
      if (!lineA || !lineB || !MeasurementGeometryMath::parallelLineDistance(*lineA, *lineB, distancePixels, &pointOnA, &pointOnB))
      {
        continue;
      }

      overlay.dimensions.append(
        measurementDimension(toPointF(pointOnA.point), toPointF(pointOnB.point), measurement, dimWidth));
      if (!compact)
      {
        overlay.points.append({toPointF(pointOnA.point), "A", QColor("#ff8a00")});
        overlay.points.append({toPointF(pointOnB.point), "H", QColor("#ff8a00")});
      }
      continue;
    }

    if (measurement.type == "circle_diameter")
    {
      const CircleGeometry* circle = findCircleByMetaId(set, measurement.sourceAId);
      if (!circle || circle->radius <= 0.0)
      {
        continue;
      }

      const cv::Point2d left(circle->center.x - circle->radius, circle->center.y);
      const cv::Point2d right(circle->center.x + circle->radius, circle->center.y);
      const cv::Point2d top(circle->center.x, circle->center.y - circle->radius);
      const cv::Point2d bottom(circle->center.x, circle->center.y + circle->radius);
      overlay.lines.append({toPointF(top), toPointF(bottom), QColor("#35c46a"), dimWidth});
      overlay.dimensions.append(measurementDimension(toPointF(left), toPointF(right), measurement, dimWidth));
      if (!compact)
      {
        overlay.points.append({toPointF(circle->center), "C", QColor("#35c46a")});
      }
      continue;
    }

    if (measurement.type == "line_line_angle")
    {
      const LineGeometry* lineA = findLineByMetaId(set, measurement.sourceAId);
      const LineGeometry* lineB = findLineByMetaId(set, measurement.sourceBId);
      if (!lineA || !lineB)
      {
        continue;
      }

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

      const double armLength = angleArmLength > 0.0
        ? angleArmLength
        : std::min(90.0, std::max(35.0, std::min(lineLength(*lineA), lineLength(*lineB)) * 0.35));
      GeometryOverlayAngle angleOverlay{
        toPointF(center),
        toPointF(center + directionA * armLength),
        toPointF(center + directionB * armLength),
        measurementLabel(measurement, "deg"),
        QColor("#ff8a00"),
        angleWidth
      };
      angleOverlay.labelColor = measurementLabelColor(measurement);
      overlay.angles.append(angleOverlay);
      if (!compact)
      {
        overlay.points.append({toPointF(center), "V", QColor("#ff8a00")});
      }
      continue;
    }
  }
}
