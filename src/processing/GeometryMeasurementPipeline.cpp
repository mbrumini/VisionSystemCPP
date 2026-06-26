#include "processing/GeometryMeasurementPipeline.h"

#include "geometry/ConstructedGeometryMath.h"
#include "measurement/MeasurementGeometryMath.h"

#include <QSet>

namespace
{
QString resolveGeometryMetaId(const QString& id)
{
  return MeasurementGeometryMath::geometrySourceMetaId(id);
}

const LineGeometry* findLineByMetaId(const GeometrySet& set, const QString& id)
{
  const QString metaId = resolveGeometryMetaId(id);
  for (const LineGeometry& line : set.lines)
  {
    if (line.meta.id == metaId)
    {
      return &line;
    }
  }
  for (const ConstructedLineGeometry& line : set.constructedLines)
  {
    if (line.line.meta.id == metaId)
    {
      return &line.line;
    }
  }
  return nullptr;
}

const PointGeometry* findPointByMetaId(const GeometrySet& set, const QString& id)
{
  const QString metaId = resolveGeometryMetaId(id);
  for (const PointGeometry& point : set.points)
  {
    if (point.meta.id == metaId)
    {
      return &point;
    }
  }
  for (const ConstructedPointGeometry& point : set.constructedPoints)
  {
    if (point.point.meta.id == metaId)
    {
      return &point.point;
    }
  }
  static thread_local PointGeometry circleCenterPoint;
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == metaId)
    {
      circleCenterPoint.point = circle.center;
      circleCenterPoint.meta = circle.meta;
      circleCenterPoint.meta.valid = true;
      return &circleCenterPoint;
    }
  }
  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == metaId)
    {
      circleCenterPoint.point = arc.center;
      circleCenterPoint.meta = arc.meta;
      circleCenterPoint.meta.valid = true;
      return &circleCenterPoint;
    }
  }
  return nullptr;
}

bool findCircleLikeByMetaId(const GeometrySet& set, const QString& id, CircleGeometry& result)
{
  const QString metaId = resolveGeometryMetaId(id);
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == metaId)
    {
      result = circle;
      return true;
    }
  }
  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == metaId)
    {
      result.meta = arc.meta;
      result.center = arc.center;
      result.radius = arc.radius;
      result.meanError = arc.meanError;
      return true;
    }
  }
  return false;
}

const CircleGeometry* findCircleByMetaId(const GeometrySet& set, const QString& id)
{
  const QString metaId = resolveGeometryMetaId(id);
  static thread_local CircleGeometry arcCircle;
  for (const CircleGeometry& circle : set.circles)
  {
    if (circle.meta.id == metaId)
    {
      return &circle;
    }
  }
  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == metaId)
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

QString measurementKey(const MeasurementRecipeConfig& config)
{
  return QString("%1|%2|%3").arg(config.type, config.sourceAId, config.sourceBId);
}

void appendMeasurementResult(GeometrySet& set,
                             const MeasurementRecipeConfig& config,
                             const CameraMeasurementCalibration& calibration,
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
  else if (calibration.enabled &&
           calibration.pixelSizeXMm > 0.000001 &&
           calibration.pixelSizeYMm > 0.000001)
  {
    const double pixelSizeMm = (calibration.pixelSizeXMm + calibration.pixelSizeYMm) * 0.5;
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
}

void rebuildConstructedGeometries(GeometrySet& set, const QVector<ConstructedGeometryRecipeConfig>& configs)
{
  set.constructedPoints.clear();
  set.constructedLines.clear();

  for (const ConstructedGeometryRecipeConfig& config : configs)
  {
    if (!config.enabled)
    {
      continue;
    }

    if (config.type == "line_line_intersection")
    {
      const LineGeometry* first = findLineByMetaId(set, config.sourceAId);
      const LineGeometry* second = findLineByMetaId(set, config.sourceBId);
      PointGeometry point;
      if (first && second && ConstructedGeometryMath::lineLineIntersection(*first, *second, point))
      {
        set.constructedPoints.append({point, first->meta.id, second->meta.id});
      }
      continue;
    }

    if (config.type == "line_circle_intersection")
    {
      const LineGeometry* line = findLineByMetaId(set, config.sourceAId);
      CircleGeometry circle;
      if (line && findCircleLikeByMetaId(set, config.sourceBId, circle))
      {
        const QVector<PointGeometry> points = ConstructedGeometryMath::lineCircleIntersections(*line, circle);
        for (const PointGeometry& point : points)
        {
          set.constructedPoints.append({point, line->meta.id, circle.meta.id});
        }
      }
      continue;
    }

    if (config.type == "circle_circle_intersection")
    {
      CircleGeometry first;
      CircleGeometry second;
      if (findCircleLikeByMetaId(set, config.sourceAId, first) &&
          findCircleLikeByMetaId(set, config.sourceBId, second))
      {
        const QVector<PointGeometry> points = ConstructedGeometryMath::circleCircleIntersections(first, second);
        for (const PointGeometry& point : points)
        {
          set.constructedPoints.append({point, first.meta.id, second.meta.id});
        }
      }
      continue;
    }

    if (config.type == "circle_center")
    {
      CircleGeometry circle;
      if (findCircleLikeByMetaId(set, config.sourceAId, circle))
      {
        const PointGeometry point = ConstructedGeometryMath::circleCenter(circle);
        set.constructedPoints.append({point, circle.meta.id, {}});
      }
      continue;
    }

    if (config.type == "midpoint")
    {
      const PointGeometry* first = findPointByMetaId(set, config.sourceAId);
      const PointGeometry* second = findPointByMetaId(set, config.sourceBId);
      if (first && second)
      {
        const PointGeometry point = ConstructedGeometryMath::midpoint(*first, *second);
        set.constructedPoints.append({point, first->meta.id, second->meta.id});
      }
      continue;
    }

    if (config.type == "offset_line")
    {
      const LineGeometry* source = findLineByMetaId(set, config.sourceAId);
      LineGeometry line;
      if (source && ConstructedGeometryMath::offsetLine(*source, config.offset, line))
      {
        set.constructedLines.append({line, source->meta.id, {}, config.offset});
      }
      continue;
    }

    if (config.type == "angle_bisector")
    {
      const LineGeometry* first = findLineByMetaId(set, config.sourceAId);
      const LineGeometry* second = findLineByMetaId(set, config.sourceBId);
      if (first && second)
      {
        const QVector<LineGeometry> lines = ConstructedGeometryMath::angleBisectors(*first, *second);
        for (const LineGeometry& line : lines)
        {
          set.constructedLines.append({line, first->meta.id, second->meta.id, 0.0});
        }
      }
      continue;
    }

    if (config.type == "tangent_line")
    {
      const PointGeometry* point = findPointByMetaId(set, config.sourceAId);
      CircleGeometry circle;
      if (point && findCircleLikeByMetaId(set, config.sourceBId, circle))
      {
        const QVector<LineGeometry> lines = ConstructedGeometryMath::tangentLinesFromPointToCircle(*point, circle);
        for (const LineGeometry& line : lines)
        {
          set.constructedLines.append({line, point->meta.id, circle.meta.id, 0.0});
        }
      }
      continue;
    }

    if (config.type == "project_point_line")
    {
      const PointGeometry* pointSource = findPointByMetaId(set, config.sourceAId);
      const LineGeometry* lineSource = findLineByMetaId(set, config.sourceBId);
      PointGeometry point;
      if (pointSource && lineSource && ConstructedGeometryMath::projectPointOntoLine(*pointSource, *lineSource, point))
      {
        set.constructedPoints.append({point, pointSource->meta.id, lineSource->meta.id});
      }
    }
  }
}

static bool isThreadMeasurementType(const QString& type)
{
  return type == QStringLiteral("thread_major_diameter") ||
         type == QStringLiteral("thread_minor_diameter") ||
         type == QStringLiteral("thread_pitch") ||
         type == QStringLiteral("thread_phase") ||
         type == QStringLiteral("thread_pitch_diameter");
}

void rebuildMeasurements(GeometrySet& set,
                         const QVector<MeasurementRecipeConfig>& configs,
                         const CameraMeasurementCalibration& calibration)
{
  QVector<MeasurementResult> threadMeasurements;
  threadMeasurements.reserve(set.measurements.size());
  for (const MeasurementResult& measurement : set.measurements)
  {
    if (isThreadMeasurementType(measurement.type))
    {
      threadMeasurements.append(measurement);
    }
  }

  set.measurements.clear();

  QSet<QString> processedMeasurementKeys;
  for (const MeasurementRecipeConfig& config : configs)
  {
    if (!config.enabled)
    {
      continue;
    }
    const QString key = measurementKey(config);
    if (processedMeasurementKeys.contains(key))
    {
      continue;
    }
    processedMeasurementKeys.insert(key);

    if (config.type == "point_point_distance")
    {
      const PointGeometry* pointA = findPointByMetaId(set, config.sourceAId);
      const PointGeometry* pointB = findPointByMetaId(set, config.sourceBId);
      double distancePixels = 0.0;
      if (pointA && pointB && MeasurementGeometryMath::pointPointDistance(*pointA, *pointB, distancePixels))
      {
        appendMeasurementResult(set, config, calibration, pointA->meta.id, pointB->meta.id, distancePixels);
      }
      else
      {
        appendFailedMeasurementResult(set, config);
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
        appendMeasurementResult(set, config, calibration, point->meta.id, line->meta.id, distancePixels);
      }
      else
      {
        appendFailedMeasurementResult(set, config);
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
        appendMeasurementResult(set, config, calibration, lineA->meta.id, lineB->meta.id, distancePixels);
      }
      else
      {
        appendFailedMeasurementResult(set, config);
      }
      continue;
    }

    if (config.type == "circle_diameter")
    {
      const CircleGeometry* circle = findCircleByMetaId(set, config.sourceAId);
      double diameterPixels = 0.0;
      if (circle && MeasurementGeometryMath::circleDiameterPixels(*circle, diameterPixels))
      {
        appendMeasurementResult(set, config, calibration, circle->meta.id, {}, diameterPixels);
      }
      else
      {
        appendFailedMeasurementResult(set, config);
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
        appendMeasurementResult(set, config, calibration, lineA->meta.id, lineB->meta.id, angleDegrees);
      }
      else
      {
        appendFailedMeasurementResult(set, config);
      }
    }
  }

  for (const MeasurementResult& measurement : threadMeasurements)
  {
    set.measurements.append(measurement);
  }
}

GeometryPipelineOutput runGeometryMeasurementPipeline(const GeometryPipelineInput& input)
{
  GeometryPipelineOutput output;
  const ConfiguredGeometryDetectOutput detected = detectConfiguredGeometries(input.geometry);

  GeometrySet set;
  set.lines = detected.lines;
  set.points = detected.points;
  set.circles = detected.circles;
  set.arcs = detected.arcs;

  rebuildConstructedGeometries(set, input.constructedConfigs);
  rebuildMeasurements(set, input.measurementConfigs, input.calibration);

  output.lines = set.lines;
  output.points = set.points;
  output.circles = set.circles;
  output.arcs = set.arcs;
  output.constructedPoints = set.constructedPoints;
  output.constructedLines = set.constructedLines;
  output.measurements = set.measurements;
  return output;
}
