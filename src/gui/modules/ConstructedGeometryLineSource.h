#pragma once

#include "geometry/LineGeometry.h"

#include <QHash>
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
QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(
  const QVector<LineGeometry>& lines,
  const QHash<QString, QString>& aliases);
QVector<ConstructedGeometryLineSource> constructedGeometryLineSources(
  const GeometrySet& set,
  const QHash<QString, QString>& aliases = QHash<QString, QString>());
const LineGeometry* findConstructedGeometryLineSource(const QVector<LineGeometry>& lines, const QString& id);
const LineGeometry* findConstructedGeometryLineSource(const GeometrySet& set, const QString& sourceId);
