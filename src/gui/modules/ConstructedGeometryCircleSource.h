#pragma once

#include "geometry/CircleGeometry.h"
#include "geometry/GeometrySet.h"

#include <QHash>
#include <QString>
#include <QVector>

struct ConstructedGeometryCircleSource
{
  QString id;
  QString label;
  const CircleGeometry* circle = nullptr;
  bool arcSource = false;
};

QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(const QVector<CircleGeometry>& circles);
QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(
  const QVector<CircleGeometry>& circles,
  const QHash<QString, QString>& aliases);
QVector<ConstructedGeometryCircleSource> constructedGeometryCircleSources(
  const GeometrySet& set,
  const QHash<QString, QString>& aliases = QHash<QString, QString>());
const CircleGeometry* findConstructedGeometryCircleSource(const QVector<CircleGeometry>& circles, const QString& id);
bool constructedGeometryCircleSourceValue(const GeometrySet& set, const QString& id, CircleGeometry& result);
