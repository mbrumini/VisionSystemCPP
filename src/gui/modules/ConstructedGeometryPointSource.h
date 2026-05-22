#pragma once

#include "geometry/PointGeometry.h"

#include <QString>
#include <QVector>

struct GeometrySet;

struct ConstructedGeometryPointSource
{
  QString id;
  QString label;
  const PointGeometry* point = nullptr;
};

QVector<ConstructedGeometryPointSource> constructedGeometryPointSources(const GeometrySet& set);
const PointGeometry* findConstructedGeometryPointSource(const GeometrySet& set, const QString& sourceId);
