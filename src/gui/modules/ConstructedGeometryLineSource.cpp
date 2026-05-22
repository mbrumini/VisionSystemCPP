#include "gui/modules/ConstructedGeometryLineSource.h"

#include "geometry/GeometrySet.h"

namespace
{
QString lineSourceId(const QString& prefix, int index, const LineGeometry& line)
{
  return QString("%1:%2:%3").arg(prefix).arg(index).arg(line.meta.id);
}

QString lineSourceLabel(const QString& prefix, int index, const LineGeometry& line)
{
  const QString id = line.meta.id.isEmpty() ? QString("%1_%2").arg(prefix).arg(index + 1) : line.meta.id;
  const QString label = line.meta.label.isEmpty() ? id : QString("%1 (%2)").arg(line.meta.label, id);
  return QString("%1 %2").arg(prefix, label);
}
}

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

QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(const GeometrySet& set)
{
  QVector<ConstructedGeometryLineSource> sources;
  sources.reserve(set.lines.size() + set.constructedLines.size());

  for (int i = 0; i < set.lines.size(); ++i)
  {
    const LineGeometry& line = set.lines[i];
    ConstructedGeometryLineSource source;
    source.id = lineSourceId("line", i, line);
    source.label = lineSourceLabel("L", i, line);
    source.line = &line;
    sources.append(source);
  }

  for (int i = 0; i < set.constructedLines.size(); ++i)
  {
    const LineGeometry& line = set.constructedLines[i].line;
    ConstructedGeometryLineSource source;
    source.id = lineSourceId("constructed", i, line);
    source.label = lineSourceLabel("C", i, line);
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

const LineGeometry* findConstructedGeometryLineSource(const GeometrySet& set, const QString& sourceId)
{
  const QVector<ConstructedGeometryLineSource> sources = constructedGeometryLineSources(set);
  for (const ConstructedGeometryLineSource& source : sources)
  {
    if (source.id == sourceId)
    {
      return source.line;
    }
  }

  return nullptr;
}
