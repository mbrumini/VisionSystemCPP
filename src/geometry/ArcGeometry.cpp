#include "ArcGeometry.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 0.000001;

double pointDistance(const cv::Point2d& a, const cv::Point2d& b)
{
  const cv::Point2d delta = a - b;
  return std::sqrt(delta.dot(delta));
}
}

double arcSpanRadians(const ArcGeometry& arc)
{
  double span = arc.endAngleRadians - arc.startAngleRadians;
  while (span < 0.0)
  {
    span += 2.0 * kPi;
  }
  return span;
}

double arcNormalizedAngle(double angleRadians)
{
  while (angleRadians < 0.0)
  {
    angleRadians += 2.0 * kPi;
  }
  while (angleRadians >= 2.0 * kPi)
  {
    angleRadians -= 2.0 * kPi;
  }
  return angleRadians;
}

cv::Point2d arcPointAt(const cv::Point2d& center, double radius, double angleRadians)
{
  return cv::Point2d(center.x + std::cos(angleRadians) * radius, center.y + std::sin(angleRadians) * radius);
}

bool arcAngleOnSpan(double startAngleRadians, double endAngleRadians, double angleRadians)
{
  const double start = arcNormalizedAngle(startAngleRadians);
  const double end = arcNormalizedAngle(endAngleRadians);
  const double angle = arcNormalizedAngle(angleRadians);
  double span = end - start;
  if (span < 0.0)
  {
    span += 2.0 * kPi;
  }
  double offset = angle - start;
  if (offset < 0.0)
  {
    offset += 2.0 * kPi;
  }
  return offset <= span + kEpsilon;
}

cv::Point2d arcStartPoint(const ArcGeometry& arc)
{
  return arcPointAt(arc.center, arc.radius, arc.startAngleRadians);
}

cv::Point2d arcEndPoint(const ArcGeometry& arc)
{
  return arcPointAt(arc.center, arc.radius, arc.endAngleRadians);
}

cv::Point2d closestPointOnArc(const ArcGeometry& arc, const cv::Point2d& point)
{
  if (arc.radius <= kEpsilon)
  {
    return arc.center;
  }

  const double radialAngle =
    std::atan2(point.y - arc.center.y, point.x - arc.center.x);
  if (arcAngleOnSpan(arc.startAngleRadians, arc.endAngleRadians, radialAngle))
  {
    return arcPointAt(arc.center, arc.radius, radialAngle);
  }

  const cv::Point2d start = arcStartPoint(arc);
  const cv::Point2d end = arcEndPoint(arc);
  return pointDistance(point, start) <= pointDistance(point, end) ? start : end;
}
