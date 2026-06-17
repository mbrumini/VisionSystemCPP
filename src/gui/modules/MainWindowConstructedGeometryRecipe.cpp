#include "gui/modules/MainWindowConstructedGeometryModule.h"

#include "geometry/ConstructedGeometryMath.h"
#include "gui/modules/ConstructedGeometryCircleSource.h"
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
  return findConstructedGeometryCircleSource(set.circles, id);
}

bool findCircleLikeByMetaId(const GeometrySet& set, const QString& id, CircleGeometry& result)
{
  return constructedGeometryCircleSourceValue(set, id, result);
}
}

void MainWindowConstructedGeometryModule::saveConstructedGeometryRecipeAction(const CameraConfig& camera,
                                                                              const QString& type,
                                                                              const QString& sourceAId,
                                                                              const QString& sourceBId,
                                                                              double offset)
{
  ConstructedGeometryRecipeConfig config;
  config.enabled = true;
  config.type = type;
  config.sourceAId = sourceAId;
  config.sourceBId = sourceBId;
  config.offset = offset;

  QString error;
  if (!recipes().appendConstructedGeometry(camera.id, config, &error))
  {
    log(QString("%1: %2").arg(tr("log.constructedRecipeSaveFailed"), error));
  }
}

void MainWindowConstructedGeometryModule::rebuildConstructedGeometryRecipe(const CameraConfig& camera)
{
  GeometrySet& set = cameraRuntime()[camera.id].geometries();
  set.constructedPoints.clear();
  set.constructedLines.clear();

  const QVector<ConstructedGeometryRecipeConfig> configs = recipes().loadConstructedGeometries(camera.id);
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
