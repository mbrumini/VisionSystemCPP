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
PointGeometry circleCenter(const CircleGeometry& circle);
PointGeometry midpoint(const PointGeometry& a, const PointGeometry& b);
bool offsetLine(const LineGeometry& source, double offset, LineGeometry& result);
QVector<LineGeometry> angleBisectors(const LineGeometry& a, const LineGeometry& b);
QVector<LineGeometry> tangentLinesFromPointToCircle(const PointGeometry& point, const CircleGeometry& circle);
bool projectPointOntoLine(const PointGeometry& point, const LineGeometry& line, PointGeometry& result);
}
