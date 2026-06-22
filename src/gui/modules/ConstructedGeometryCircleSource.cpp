#include "gui/modules/ConstructedGeometryCircleSource.h"

#include "geometry/GeometrySet.h"
#include "gui/geometry/GeometryDisplayNames.h"

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const QVector<CircleGeometry>& circles)
{
  return constructedGeometryCircleSources(circles, QHash<QString, QString>());
}

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const QVector<CircleGeometry>& circles,
                                                                          const QHash<QString, QString>& aliases)
{
  QVector<ConstructedGeometryCircleSource> sources;
  sources.reserve(circles.size());

  for (const CircleGeometry& circle : circles)
  {
    ConstructedGeometryCircleSource source;
    source.id = circle.meta.id;
    source.label = GeometryDisplayNames::circleSourceLabel(circle, aliases);
    source.circle = &circle;
    sources.append(source);
  }

  return sources;
}

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const GeometrySet& set,
                                                                          const QHash<QString, QString>& aliases)
{
  QVector<ConstructedGeometryCircleSource> sources = constructedGeometryCircleSources(set.circles, aliases);
  sources.reserve(set.circles.size() + set.arcs.size());

  for (const ArcGeometry& arc : set.arcs)
  {
    CircleGeometry circle;
    circle.meta = arc.meta;
    circle.center = arc.center;
    circle.radius = arc.radius;
    circle.meanError = arc.meanError;

    ConstructedGeometryCircleSource source;
    source.id = arc.meta.id;
    source.label = GeometryDisplayNames::circleSourceLabel(circle, aliases, true);
    source.arcSource = true;
    sources.append(source);
  }

  return sources;
}

const CircleGeometry* findConstructedGeometryCircleSource(const QVector<CircleGeometry>& circles, const QString& id)
{
  for (const CircleGeometry& circle : circles)
  {
    if (circle.meta.id == id)
    {
      return &circle;
    }
  }

  return nullptr;
}

bool constructedGeometryCircleSourceValue(const GeometrySet& set, const QString& id, CircleGeometry& result)
{
  if (const CircleGeometry* circle = findConstructedGeometryCircleSource(set.circles, id))
  {
    result = *circle;
    return true;
  }

  for (const ArcGeometry& arc : set.arcs)
  {
    if (arc.meta.id == id)
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
