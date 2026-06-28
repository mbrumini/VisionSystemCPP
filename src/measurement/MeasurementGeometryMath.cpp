#include "measurement/MeasurementGeometryMath.h"

#include "geometry/ConstructedGeometryMath.h"

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

cv::Point2d lineSegmentMidpoint(const LineGeometry& line)
{
  return (line.start + line.end) * 0.5;
}

bool alignedParallelDirection(const cv::Point2d& directionA, cv::Point2d& directionB)
{
  if (directionB.dot(directionA) < 0.0)
  {
    directionB *= -1.0;
  }

  cv::Point2d average = directionA + directionB;
  const double averageLength = std::sqrt(average.dot(average));
  if (averageLength < kEpsilon)
  {
    return false;
  }

  average *= 1.0 / averageLength;
  directionB = average;
  return true;
}

bool orientedRayFromVertex(const LineGeometry& line, const cv::Point2d& vertex, cv::Point2d& unitRay)
{
  const cv::Point2d toStart = line.start - vertex;
  const cv::Point2d toEnd = line.end - vertex;
  const double lenStartSq = toStart.dot(toStart);
  const double lenEndSq = toEnd.dot(toEnd);
  cv::Point2d ray = (lenEndSq >= lenStartSq) ? toEnd : toStart;
  const double length = std::sqrt(ray.dot(ray));
  if (length < kEpsilon)
  {
    return false;
  }
  unitRay = ray * (1.0 / length);
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
  return parallelLineDistance(
    lineA,
    lineB,
    ParallelLineDistanceMode::Average,
    distancePixels,
    pointOnLineA,
    pointOnLineB);
}

bool parallelLineDistance(const LineGeometry& lineA,
                          const LineGeometry& lineB,
                          ParallelLineDistanceMode mode,
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

  cv::Point2d sharedDirection = directionB;
  if (!alignedParallelDirection(directionA, sharedDirection))
  {
    return false;
  }

  const cv::Point2d normal(-sharedDirection.y, sharedDirection.x);
  const cv::Point2d anchorOnA = lineSegmentMidpoint(lineA);
  const cv::Point2d midpointB = lineSegmentMidpoint(lineB);
  const double signedDistance = (midpointB - anchorOnA).dot(normal);
  const double tOnB = (anchorOnA - midpointB).dot(sharedDirection);
  const cv::Point2d footOnB = midpointB + sharedDirection * tOnB;

  if (mode == ParallelLineDistanceMode::Average)
  {
    distancePixels = std::abs(signedDistance);
  }
  else
  {
    if (lineA.edgePoints.empty() || lineB.edgePoints.empty())
    {
      return false;
    }

    const double side = signedDistance < 0.0 ? -1.0 : 1.0;
    double minA = 0.0;
    double maxA = 0.0;
    double minB = 0.0;
    double maxB = 0.0;
    bool firstA = true;
    bool firstB = true;
    for (const cv::Point2d& point : lineA.edgePoints)
    {
      const double value = (point - anchorOnA).dot(normal) * side;
      if (firstA)
      {
        minA = value;
        maxA = value;
        firstA = false;
      }
      else
      {
        minA = std::min(minA, value);
        maxA = std::max(maxA, value);
      }
    }
    for (const cv::Point2d& point : lineB.edgePoints)
    {
      const double value = (point - anchorOnA).dot(normal) * side;
      if (firstB)
      {
        minB = value;
        maxB = value;
        firstB = false;
      }
      else
      {
        minB = std::min(minB, value);
        maxB = std::max(maxB, value);
      }
    }

    if (firstA || firstB)
    {
      return false;
    }

    if (mode == ParallelLineDistanceMode::Minimum)
    {
      distancePixels = std::max(0.0, minB - maxA);
    }
    else
    {
      distancePixels = std::max(0.0, maxB - minA);
    }
  }

  const double score = std::min(lineA.meta.score, lineB.meta.score);
  const bool valid = lineA.meta.valid && lineB.meta.valid;
  if (pointOnLineA)
  {
    fillMeasurementPoint(*pointOnLineA,
                         anchorOnA,
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
  PointGeometry intersection;
  if (ConstructedGeometryMath::lineLineIntersection(lineA, lineB, intersection))
  {
    cv::Point2d rayA;
    cv::Point2d rayB;
    if (orientedRayFromVertex(lineA, intersection.point, rayA) &&
        orientedRayFromVertex(lineB, intersection.point, rayB))
    {
      const double dot = std::clamp(rayA.dot(rayB), -1.0, 1.0);
      angleDegrees = std::acos(dot) * 180.0 / 3.14159265358979323846;
      return true;
    }
  }

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

bool lineLineAngleGeometry(const LineGeometry& lineA,
                           const LineGeometry& lineB,
                           double& angleDegrees,
                           cv::Point2d& vertex,
                           cv::Point2d& rayA,
                           cv::Point2d& rayB)
{
  PointGeometry intersection;
  if (!ConstructedGeometryMath::lineLineIntersection(lineA, lineB, intersection))
  {
    return false;
  }

  vertex = intersection.point;
  if (!orientedRayFromVertex(lineA, vertex, rayA) || !orientedRayFromVertex(lineB, vertex, rayB))
  {
    return false;
  }

  const double dot = std::clamp(rayA.dot(rayB), -1.0, 1.0);
  angleDegrees = std::acos(dot) * 180.0 / 3.14159265358979323846;
  return true;
}
QString geometrySourceMetaId(const QString& sourceId)
{
  const int separator = sourceId.lastIndexOf(':');
  if (separator <= 0)
  {
    return sourceId;
  }

  const QString suffix = sourceId.mid(separator + 1);
  if (suffix.startsWith("line_") ||
      suffix.startsWith("point_") ||
      suffix.startsWith("circle_") ||
      suffix.startsWith("constructed_") ||
      suffix.startsWith("arc_"))
  {
    return suffix;
  }
  return sourceId;
}
}
