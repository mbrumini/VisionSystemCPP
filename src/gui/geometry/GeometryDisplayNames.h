#pragma once

#include "geometry/CircleGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

#include <QHash>
#include <QString>

namespace GeometryDisplayNames
{
QString formatLabel(const QString& geometryId, const QString& labelOrAlias);

QString resolvedLabel(const QString& geometryId,
                      const QString& metaLabel,
                      const QHash<QString, QString>& aliases);

QString lineSourceLabel(const QString& prefix,
                        int index,
                        const LineGeometry& line,
                        const QHash<QString, QString>& aliases);

QString pointSourceLabel(const QString& prefix,
                         int index,
                         const PointGeometry& point,
                         const QHash<QString, QString>& aliases);

QString circleSourceLabel(const CircleGeometry& circle,
                          const QHash<QString, QString>& aliases,
                          bool arcSource = false);
}
