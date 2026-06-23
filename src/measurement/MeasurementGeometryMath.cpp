#include "measurement/MeasurementGeometryMath.h"

#include <algorithm>
#include <cmath>

namespace MeasurementGeometryMath
{
namespace
{
constexpr double kEpsilon = 0.000001;

double cross(const cv::Point2d& a, const cv::Point2d& b)
{
  return a.x * b.y - a.y * b.x;
}

bool normalizedDirection(const LineGeometry& line, cv::Point2d& direction, double& length)
{
  direction = line.end - line.start;
  length = std::sqrt(direction.dot(direction));
  if (length < kEpsilon)
  {
    return false;
  }
  direction *= 1.0 / length;
  return true;
}

void fillMeasurementPoint(PointGeometry& point,
                          const cv::Point2d& imagePoint,
                          const QString& id,
                          const QString& method,
                          double score,
                          bool valid)
{
  point.point = imagePoint;
  point.meta.id = id;
  point.meta.label = id;
  point.meta.method = method;
  point.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  point.meta.score = score;
  point.meta.valid = valid;
}
}

bool pointPointDistance(const PointGeometry& pointA, const PointGeometry& pointB, double& distancePixels)
{
  const cv::Point2d delta = pointA.point - pointB.point;
  distancePixels = std::sqrt(delta.dot(delta));
  return true;
}

bool pointLineDistance(const PointGeometry& point, const LineGeometry& line, double& distancePixels, PointGeometry* projectedPoint)
{
  const cv::Point2d direction = line.end - line.start;
  const double lengthSquared = direction.dot(direction);
  if (lengthSquared < kEpsilon)
  {
    return false;
  }

  const double t = (point.point - line.start).dot(direction) / lengthSquared;
  const cv::Point2d projection = line.start + direction * t;
  const cv::Point2d delta = point.point - projection;
  distancePixels = std::sqrt(delta.dot(delta));

  if (projectedPoint)
  {
    fillMeasurementPoint(*projectedPoint,
                         projection,
                         QString("%1_distance_foot_%2").arg(point.meta.id, line.meta.id),
                         "measurement_point_line_projection",
                         std::min(point.meta.score, line.meta.score),
                         point.meta.valid && line.meta.valid);
  }

  return true;
}

bool parallelLineDistance(const LineGeometry& lineA,
                          const LineGeometry& lineB,
                          double& distancePixels,
                          PointGeometry* pointOnLineA,
                          PointGeometry* pointOnLineB)
{
  cv::Point2d directionA;
  cv::Point2d directionB;
  double lengthA = 0.0;
  double lengthB = 0.0;
  if (!normalizedDirection(lineA, directionA, lengthA) || !normalizedDirection(lineB, directionB, lengthB))
  {
    return false;
  }

  if (std::abs(cross(directionA, directionB)) > 0.02)
  {
    return false;
  }

  const cv::Point2d normal(-directionB.y, directionB.x);
  const double signedDistance = (lineA.start - lineB.start).dot(normal);
  const cv::Point2d footOnB = lineA.start - normal * signedDistance;
  distancePixels = std::abs(signedDistance);

  const double score = std::min(lineA.meta.score, lineB.meta.score);
  const bool valid = lineA.meta.valid && lineB.meta.valid;
  if (pointOnLineA)
  {
    fillMeasurementPoint(*pointOnLineA,
                         lineA.start,
                         QString("%1_distance_anchor_%2").arg(lineA.meta.id, lineB.meta.id),
                         "measurement_line_line_anchor",
                         score,
                         valid);
  }
  if (pointOnLineB)
  {
    fillMeasurementPoint(*pointOnLineB,
                         footOnB,
                         QString("%1_distance_foot_%2").arg(lineA.meta.id, lineB.meta.id),
                         "measurement_line_line_projection",
                         score,
                         valid);
  }

  return true;
}

bool circleDiameterPixels(const CircleGeometry& circle, double& diameterPixels)
{
  if (circle.radius <= kEpsilon)
  {
    return false;
  }
  diameterPixels = circle.radius * 2.0;
  return true;
}

bool lineLineAngleDegrees(const LineGeometry& lineA, const LineGeometry& lineB, double& angleDegrees)
{
  cv::Point2d directionA;
  cv::Point2d directionB;
  double lengthA = 0.0;
  double lengthB = 0.0;
  if (!normalizedDirection(lineA, directionA, lengthA) || !normalizedDirection(lineB, directionB, lengthB))
  {
    return false;
  }

  const double dot = std::clamp(std::abs(directionA.dot(directionB)), 0.0, 1.0);
  angleDegrees = std::acos(dot) * 180.0 / 3.14159265358979323846;
  return true;
}
}
