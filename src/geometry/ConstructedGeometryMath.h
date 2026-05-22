#pragma once

#include "geometry/CircleGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

#include <QVector>

namespace ConstructedGeometryMath
{
bool lineLineIntersection(const LineGeometry& a, const LineGeometry& b, PointGeometry& result);
QVector<PointGeometry> lineCircleIntersections(const LineGeometry& line, const CircleGeometry& circle);
QVector<PointGeometry> circleCircleIntersections(const CircleGeometry& a, const CircleGeometry& b);
}
