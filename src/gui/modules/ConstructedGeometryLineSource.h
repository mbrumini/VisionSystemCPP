#pragma once

#include "geometry/LineGeometry.h"

#include <QString>
#include <QVector>

struct GeometrySet;

struct ConstructedGeometryLineSource
{
  QString id;
  QString label;
  const LineGeometry* line = nullptr;
};

QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(const QVector<LineGeometry>& lines);
QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(const GeometrySet& set);
const LineGeometry* findConstructedGeometryLineSource(const QVector<LineGeometry>& lines, const QString& id);
const LineGeometry* findConstructedGeometryLineSource(const GeometrySet& set, const QString& sourceId);
