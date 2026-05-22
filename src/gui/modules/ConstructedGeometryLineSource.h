#pragma once

#include "geometry/LineGeometry.h"

#include <QString>
#include <QVector>

struct ConstructedGeometryLineSource
{
  QString id;
  QString label;
  const LineGeometry* line = nullptr;
};

QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(const QVector<LineGeometry>& lines);
const LineGeometry* findConstructedGeometryLineSource(const QVector<LineGeometry>& lines, const QString& id);
