#include "gui/modules/ConstructedGeometryPointSource.h"

#include "geometry/GeometrySet.h"

namespace
{
QString pointSourceId(const QString& prefix, int index, const PointGeometry& point)
{
  return QString("%1:%2:%3").arg(prefix).arg(index).arg(point.meta.id);
}

QString pointSourceLabel(const QString& prefix, int index, const PointGeometry& point)
{
  const QString id = point.meta.id.isEmpty() ? QString("%1_%2").arg(prefix).arg(index + 1) : point.meta.id;
  const QString label = point.meta.label.isEmpty() ? id : QString("%1 (%2)").arg(point.meta.label, id);
  return QString("%1 %2").arg(prefix, label);
}
}

QVector<ConstructedGeometryPointSource> constructedGeometryPointSources(const GeometrySet& set)
{
  QVector<ConstructedGeometryPointSource> sources;
  sources.reserve(set.points.size() + set.constructedPoints.size());

  for (int i = 0; i < set.points.size(); ++i)
  {
    const PointGeometry& point = set.points[i];
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("point", i, point);
    source.label = pointSourceLabel("P", i, point);
    source.point = &point;
    sources.append(source);
  }

  for (int i = 0; i < set.constructedPoints.size(); ++i)
  {
    const PointGeometry& point = set.constructedPoints[i].point;
    ConstructedGeometryPointSource source;
    source.id = pointSourceId("constructed", i, point);
    source.label = pointSourceLabel("C", i, point);
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
