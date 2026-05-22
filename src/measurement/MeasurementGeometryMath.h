#pragma once

#include "geometry/CircleGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

namespace MeasurementGeometryMath
{
bool pointPointDistance(const PointGeometry& pointA, const PointGeometry& pointB, double& distancePixels);
bool pointLineDistance(const PointGeometry& point, const LineGeometry& line, double& distancePixels, PointGeometry* projectedPoint = nullptr);
bool parallelLineDistance(const LineGeometry& lineA,
                          const LineGeometry& lineB,
                          double& distancePixels,
                          PointGeometry* pointOnLineA = nullptr,
                          PointGeometry* pointOnLineB = nullptr);
bool circleDiameterPixels(const CircleGeometry& circle, double& diameterPixels);
bool lineLineAngleDegrees(const LineGeometry& lineA, const LineGeometry& lineB, double& angleDegrees);
}
