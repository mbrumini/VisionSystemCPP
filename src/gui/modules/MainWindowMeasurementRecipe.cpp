#include "gui/modules/MainWindowMeasurementModule.h"

#include "measurement/MeasurementGeometryMath.h"
#include "runtime/CameraRuntime.h"

namespace
{
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
  return nullptr;
}

const CircleGeometry* findCircleByMetaId(const GeometrySet& set, const QString& id)
{
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == id)
    {
      return &circle;
    }
  }
  return nullptr;
}

void appendMeasurementResult(GeometrySet& set,
                             const MeasurementRecipeConfig& config,
                             const QString& sourceAId,
                             const QString& sourceBId,
                             double value)
{
  MeasurementResult result;
  result.id = config.id;
  result.type = config.type;
  result.sourceAId = sourceAId;
  result.sourceBId = sourceBId;
  result.valuePixels = value;
  result.valid = true;
  set.measurements.append(result);
}
}

void MainWindowMeasurementModule::saveMeasurementRecipeAction(const CameraConfig& camera,
                                                              const QString& type,
                                                              const QString& sourceAId,
                                                              const QString& sourceBId)
{
  MeasurementRecipeConfig config;
  config.enabled = true;
  config.type = type;
  config.sourceAId = sourceAId;
  config.sourceBId = sourceBId;

  QString error;
  if (!recipes().appendMeasurement(camera.id, config, &error))
  {
    log(QString("%1: %2").arg(tr("log.measurementRecipeSaveFailed"), error));
  }
}

void MainWindowMeasurementModule::rebuildMeasurementRecipe(const CameraConfig& camera)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  set.measurements.clear();

  const QVector<MeasurementRecipeConfig> configs = recipes().loadMeasurements(camera.id);
  for (const MeasurementRecipeConfig& config : configs)
  {
    if (!config.enabled)
    {
      continue;
    }

    if (config.type == "point_point_distance")
    {
      const PointGeometry* pointA = findPointByMetaId(set, config.sourceAId);
      const PointGeometry* pointB = findPointByMetaId(set, config.sourceBId);
      double distancePixels = 0.0;
      if (pointA && pointB && MeasurementGeometryMath::pointPointDistance(*pointA, *pointB, distancePixels))
      {
        appendMeasurementResult(set, config, pointA->meta.id, pointB->meta.id, distancePixels);
      }
      continue;
    }

    if (config.type == "point_line_distance")
    {
      const PointGeometry* point = findPointByMetaId(set, config.sourceAId);
      const LineGeometry* line = findLineByMetaId(set, config.sourceBId);
      double distancePixels = 0.0;
      if (point && line && MeasurementGeometryMath::pointLineDistance(*point, *line, distancePixels))
      {
        appendMeasurementResult(set, config, point->meta.id, line->meta.id, distancePixels);
      }
      continue;
    }

    if (config.type == "line_line_distance")
    {
      const LineGeometry* lineA = findLineByMetaId(set, config.sourceAId);
      const LineGeometry* lineB = findLineByMetaId(set, config.sourceBId);
      double distancePixels = 0.0;
      if (lineA && lineB && MeasurementGeometryMath::parallelLineDistance(*lineA, *lineB, distancePixels))
      {
        appendMeasurementResult(set, config, lineA->meta.id, lineB->meta.id, distancePixels);
      }
      continue;
    }

    if (config.type == "circle_diameter")
    {
      const CircleGeometry* circle = findCircleByMetaId(set, config.sourceAId);
      double diameterPixels = 0.0;
      if (circle && MeasurementGeometryMath::circleDiameterPixels(*circle, diameterPixels))
      {
        appendMeasurementResult(set, config, circle->meta.id, {}, diameterPixels);
      }
      continue;
    }

    if (config.type == "line_line_angle")
    {
      const LineGeometry* lineA = findLineByMetaId(set, config.sourceAId);
      const LineGeometry* lineB = findLineByMetaId(set, config.sourceBId);
      double angleDegrees = 0.0;
      if (lineA && lineB && MeasurementGeometryMath::lineLineAngleDegrees(*lineA, *lineB, angleDegrees))
      {
        appendMeasurementResult(set, config, lineA->meta.id, lineB->meta.id, angleDegrees);
      }
      continue;
    }
  }
}
