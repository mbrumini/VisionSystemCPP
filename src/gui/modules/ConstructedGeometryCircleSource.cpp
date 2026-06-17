#include "gui/modules/ConstructedGeometryCircleSource.h"

#include "geometry/GeometrySet.h"

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const QVector<CircleGeometry>& circles)
{
  QVector<ConstructedGeometryCircleSource> sources;
  sources.reserve(circles.size());

  for (const CircleGeometry& circle : circles)
  {
    ConstructedGeometryCircleSource source;
    source.id = circle.meta.id;
    source.label = circle.meta.label.isEmpty() ? circle.meta.id : QString("%1 (%2)").arg(circle.meta.label, circle.meta.id);
    source.circle = &circle;
    sources.append(source);
  }

  return sources;
}

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const GeometrySet& set)
{
  QVector<ConstructedGeometryCircleSource> sources = constructedGeometryCircleSources(set.circles);
  sources.reserve(set.circles.size() + set.arcs.size());

  for (const ArcGeometry& arc : set.arcs)
  {
    ConstructedGeometryCircleSource source;
    source.id = arc.meta.id;
    const QString label = arc.meta.label.isEmpty() ? arc.meta.id : QString("%1 (%2)").arg(arc.meta.label, arc.meta.id);
    source.label = QString("Arc %1").arg(label);
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
