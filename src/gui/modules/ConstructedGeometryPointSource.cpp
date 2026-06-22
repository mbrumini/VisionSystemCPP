#include "gui/modules/ConstructedGeometryPointSource.h"

#include "geometry/GeometrySet.h"
#include "gui/geometry/GeometryDisplayNames.h"

namespace
{
QString pointSourceId(const QString& prefix, int index, const PointGeometry& point)
{
  return QString("%1:%2:%3").arg(prefix).arg(index).arg(point.meta.id);
}
}

QVector<ConstructedGeometryPointSource> constructedGeometryPointSources(const GeometrySet& set,
                                                                        const QHash<QString, QString>& aliases)
{
  QVector<ConstructedGeometryPointSource> sources;
  sources.reserve(set.points.size() + set.constructedPoints.size());

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
