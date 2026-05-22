#pragma once

#include "geometry/CircleGeometry.h"

#include <QString>
#include <QVector>

struct ConstructedGeometryCircleSource
{
  QString id;
  QString label;
  const CircleGeometry* circle = nullptr;
};

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const QVector<CircleGeometry>& circles);
const CircleGeometry* findConstructedGeometryCircleSource(const QVector<CircleGeometry>& circles, const QString& id);
