#pragma once

#include "geometry/CircleGeometry.h"
#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

#include <QString>

namespace MeasurementGeometryMath
{
enum class ParallelLineDistanceMode
{
  Average,
  Minimum,
  Maximum
};

bool pointPointDistance(const PointGeometry& pointA, const PointGeometry& pointB, double& distancePixels);
bool pointLineDistance(const PointGeometry& point, const LineGeometry& line, double& distancePixels, PointGeometry* projectedPoint = nullptr);
bool parallelLineDistance(const LineGeometry& lineA,
                          const LineGeometry& lineB,
                          double& distancePixels,
                          PointGeometry* pointOnLineA = nullptr,
                          PointGeometry* pointOnLineB = nullptr);
bool parallelLineDistance(const LineGeometry& lineA,
                          const LineGeometry& lineB,
                          ParallelLineDistanceMode mode,
                          double& distancePixels,
                          PointGeometry* pointOnLineA = nullptr,
                          PointGeometry* pointOnLineB = nullptr);
bool circleDiameterPixels(const CircleGeometry& circle, double& diameterPixels);
bool lineLineAngleDegrees(const LineGeometry& lineA, const LineGeometry& lineB, double& angleDegrees);

// Corner angle at intersection with oriented rays for overlay.
bool lineLineAngleGeometry(const LineGeometry& lineA,
                           const LineGeometry& lineB,
                           double& angleDegrees,
                           cv::Point2d& vertex,
                           cv::Point2d& rayA,
                           cv::Point2d& rayB);

// Resolves combo ids like "line:0:line_1" to "line_1".
QString geometrySourceMetaId(const QString& sourceId);
}
