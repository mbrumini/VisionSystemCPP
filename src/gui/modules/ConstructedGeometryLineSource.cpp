#include "gui/modules/ConstructedGeometryLineSource.h"

QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(const QVector<LineGeometry>& lines)
{
  QVector<ConstructedGeometryLineSource> sources;
  sources.reserve(lines.size());

  for (const LineGeometry& line : lines)
  {
    ConstructedGeometryLineSource source;
    source.id = line.meta.id;
    source.label = line.meta.label.isEmpty() ? line.meta.id : QString("%1 (%2)").arg(line.meta.label, line.meta.id);
    source.line = &line;
    sources.append(source);
  }

  return sources;
}

const LineGeometry* findConstructedGeometryLineSource(const QVector<LineGeometry>& lines, const QString& id)
{
  for (const LineGeometry& line : lines)
  {
    if (line.meta.id == id)
    {
      return &line;
    }
  }

  return nullptr;
}
