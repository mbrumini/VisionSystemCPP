#include "gui/modules/ConstructedGeometryCircleSource.h"

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
