#include "gui/geometry/GeometryDisplayNames.h"

namespace GeometryDisplayNames
{
QString formatLabel(const QString& geometryId, const QString& labelOrAlias)
{
  const QString label = labelOrAlias.trimmed();
  if (label.isEmpty() || label == geometryId)
  {
    return geometryId;
  }

  return QString("%1 (%2)").arg(label, geometryId);
}

QString resolvedLabel(const QString& geometryId,
                      const QString& metaLabel,
                      const QHash<QString, QString>& aliases)
{
  QString label = metaLabel.trimmed();
  if (label.isEmpty() || label == geometryId)
  {
    label = aliases.value(geometryId).trimmed();
  }

  if (geometryId.endsWith("_center"))
  {
    const QString parentId = geometryId.left(geometryId.size() - QString("_center").size());
    const QString parentAlias = aliases.value(parentId).trimmed();
    if (!parentAlias.isEmpty())
    {
      return formatLabel(geometryId, QString("%1 centro").arg(parentAlias));
    }
  }

  return formatLabel(geometryId, label);
}

QString lineSourceLabel(const QString& prefix,
                        int index,
                        const LineGeometry& line,
                        const QHash<QString, QString>& aliases)
{
  const QString id = line.meta.id.isEmpty() ? QString("%1_%2").arg(prefix).arg(index + 1) : line.meta.id;
  const QString label = resolvedLabel(id, line.meta.label, aliases);
  return QString("%1 %2").arg(prefix, label);
}

QString pointSourceLabel(const QString& prefix,
                         int index,
                         const PointGeometry& point,
                         const QHash<QString, QString>& aliases)
{
  const QString id = point.meta.id.isEmpty() ? QString("%1_%2").arg(prefix).arg(index + 1) : point.meta.id;
  const QString label = resolvedLabel(id, point.meta.label, aliases);
  return QString("%1 %2").arg(prefix, label);
}

QString circleSourceLabel(const CircleGeometry& circle,
                          const QHash<QString, QString>& aliases,
                          bool arcSource)
{
  const QString label = resolvedLabel(circle.meta.id, circle.meta.label, aliases);
  return arcSource ? QString("Arc %1").arg(label) : label;
}
}
