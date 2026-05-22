#pragma once

#include "geometry/ArcGeometry.h"
#include "geometry/CircleGeometry.h"
#include "geometry/ConstructedGeometry.h"
#include "geometry/ContourGeometry.h"
#include "geometry/EdgeGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"
#include "measurement/MeasurementGeometry.h"

#include <QVector>

struct GeometrySet
{
  QVector<PointGeometry> points;
  QVector<LineGeometry> lines;
  QVector<CircleGeometry> circles;
  QVector<ArcGeometry> arcs;
  QVector<ConstructedPointGeometry> constructedPoints;
  QVector<ConstructedLineGeometry> constructedLines;
  QVector<EdgeGeometry> edges;
  QVector<ContourGeometry> contours;
  QVector<MeasurementResult> measurements;

  void clear();
  bool empty() const;
};
