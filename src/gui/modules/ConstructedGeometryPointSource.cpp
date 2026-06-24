#include "gui/modules/ConstructedGeometryPointSource.h"

#include "geometry/GeometrySet.h"
#include "gui/geometry/GeometryDisplayNames.h"

#include <QHash>

#include "geometry/ArcGeometry.h"
#include "geometry/CircleGeometry.h"

namespace
{
thread_local QVector<PointGeometry> g_syntheticMeasurePoints;

QString pointSourceId(const QString& prefix, int index, const PointGeometry& point)
{
  return QString("%1:%2:%3").arg(prefix).arg(index).arg(point.meta.id);
}

PointGeometry centerAsPoint(const GeometryMeta& meta, const cv::Point2d& center)
{
  PointGeometry point;
  point.point = center;
  point.meta = meta;
  point.meta.valid = true;
  return point;
}
}

QVector<ConstructedGeometryPointSource> constructedGeometryPointSources(const GeometrySet& set,
                                                                        const QHash<QString, QString>& aliases)
{
  g_syntheticMeasurePoints.clear();
  QVector<ConstructedGeometryPointSource> sources;
  sources.reserve(set.points.size() + set.constructedPoints.size() + set.circles.size() + set.arcs.size());

  for (int i = 0; i < set.points.size(); ++i)
  {
    const PointGeometry& point = set.points[i];
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("point", i, point);
    source.label = GeometryDisplayNames::pointSourceLabel("P", i, point, aliases);
    source.point = &point;
    sources.append(source);
  }

  for (int i = 0; i < set.constructedPoints.size(); ++i)
  {
    const PointGeometry& point = set.constructedPoints[i].point;
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("constructed", i, point);
    source.label = GeometryDisplayNames::pointSourceLabel("C", i, point, aliases);
    source.point = &point;
    sources.append(source);
  }

  for (int i = 0; i < set.circles.size(); ++i)
  {
    const CircleGeometry& circle = set.circles[i];
    g_syntheticMeasurePoints.append(centerAsPoint(circle.meta, circle.center));
    const PointGeometry& point = g_syntheticMeasurePoints.last();
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("circle", i, point);
    source.label = GeometryDisplayNames::pointSourceLabel("O", i, point, aliases);
    source.point = &point;
    sources.append(source);
  }

  for (int i = 0; i < set.arcs.size(); ++i)
  {
    const ArcGeometry& arc = set.arcs[i];
    g_syntheticMeasurePoints.append(centerAsPoint(arc.meta, arc.center));
    const PointGeometry& point = g_syntheticMeasurePoints.last();
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("arc", i, point);
    source.label = GeometryDisplayNames::pointSourceLabel("A", i, point, aliases);
    source.point = &point;
    sources.append(source);
  }

  return sources;
}

const PointGeometry* findConstructedGeometryPointSource(const GeometrySet& set, const QString& sourceId)
{
  const QVector<ConstructedGeometryPointSource> sources = constructedGeometryPointSources(set);
  for (const ConstructedGeometryPointSource& source : sources)
  {
    if (source.id == sourceId)
    {
      return source.point;
    }
  }

  return nullptr;
}
