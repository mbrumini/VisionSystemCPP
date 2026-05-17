#pragma once

#include "geometry/ArcGeometry.h"
#include "geometry/CircleGeometry.h"
#include "geometry/ContourGeometry.h"
#include "geometry/EdgeGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

#include <QVector>

struct GeometrySet
{
  QVector<PointGeometry> points;
  QVector<LineGeometry> lines;
  QVector<CircleGeometry> circles;
  QVector<ArcGeometry> arcs;
  QVector<EdgeGeometry> edges;
  QVector<ContourGeometry> contours;

  void clear();
  bool empty() const;
};
