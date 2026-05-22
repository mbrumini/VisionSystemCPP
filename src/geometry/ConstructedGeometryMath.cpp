#include "geometry/ConstructedGeometryMath.h"

#include <algorithm>
#include <cmath>

namespace ConstructedGeometryMath
{
bool lineLineIntersection(const LineGeometry& a, const LineGeometry& b, PointGeometry& result)
{
  const cv::Point2d r = a.end - a.start;
  const cv::Point2d s = b.end - b.start;
  const double denom = r.x * s.y - r.y * s.x;
  if (std::abs(denom) < 0.000001)
  {
    return false;
  }

  const cv::Point2d delta = b.start - a.start;
  const double t = (delta.x * s.y - delta.y * s.x) / denom;
  result.point = a.start + r * t;
  result.meta.id = QString("%1_x_%2").arg(a.meta.id, b.meta.id);
  result.meta.label = result.meta.id;
  result.meta.method = "constructed_line_line_intersection";
  result.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.meta.valid = true;
  result.meta.score = std::min(a.meta.score, b.meta.score);
  return true;
}

QVector<PointGeometry> lineCircleIntersections(const LineGeometry& line, const CircleGeometry& circle)
{
  QVector<PointGeometry> results;
  const cv::Point2d direction = line.end - line.start;
  const double directionLengthSquared = direction.dot(direction);
  if (directionLengthSquared < 0.000001 || circle.radius <= 0.0)
  {
    return results;
  }

  const cv::Point2d fromCenter = line.start - circle.center;
  const double a = directionLengthSquared;
  const double b = 2.0 * fromCenter.dot(direction);
  const double c = fromCenter.dot(fromCenter) - circle.radius * circle.radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < -0.000001)
  {
    return results;
  }

  const double clampedDiscriminant = std::max(0.0, discriminant);
  const double root = std::sqrt(clampedDiscriminant);
  const double firstT = (-b - root) / (2.0 * a);
  const double secondT = (-b + root) / (2.0 * a);

  const auto appendPoint = [&](double t, int index) {
    PointGeometry point;
    point.point = line.start + direction * t;
    point.meta.id = QString("%1_x_%2_%3").arg(line.meta.id, circle.meta.id).arg(index);
    point.meta.label = point.meta.id;
    point.meta.method = "constructed_line_circle_intersection";
    point.meta.coordinateSpace = GeometryCoordinateSpace::Image;
    point.meta.valid = true;
    point.meta.score = std::min(line.meta.score, circle.meta.score);
    results.append(point);
  };

  appendPoint(firstT, 1);
  if (std::abs(secondT - firstT) > 0.000001)
  {
    appendPoint(secondT, 2);
  }

  return results;
}

QVector<PointGeometry> circleCircleIntersections(const CircleGeometry& a, const CircleGeometry& b)
{
  QVector<PointGeometry> results;
  if (a.radius <= 0.0 || b.radius <= 0.0)
  {
    return results;
  }

  const cv::Point2d delta = b.center - a.center;
  const double distance = std::sqrt(delta.dot(delta));
  constexpr double epsilon = 0.000001;
  if (distance < epsilon ||
      distance > a.radius + b.radius + epsilon ||
      distance < std::abs(a.radius - b.radius) - epsilon)
  {
    return results;
  }

  const double alongCenterLine =
    (a.radius * a.radius - b.radius * b.radius + distance * distance) / (2.0 * distance);
  const double heightSquared = a.radius * a.radius - alongCenterLine * alongCenterLine;
  if (heightSquared < -epsilon)
  {
    return results;
  }

  const cv::Point2d unit = delta * (1.0 / distance);
  const cv::Point2d base = a.center + unit * alongCenterLine;
  const cv::Point2d normal(-unit.y, unit.x);
  const double height = std::sqrt(std::max(0.0, heightSquared));

  const auto appendPoint = [&](const cv::Point2d& value, int index) {
    PointGeometry point;
    point.point = value;
    point.meta.id = QString("%1_x_%2_%3").arg(a.meta.id, b.meta.id).arg(index);
    point.meta.label = point.meta.id;
    point.meta.method = "constructed_circle_circle_intersection";
    point.meta.coordinateSpace = GeometryCoordinateSpace::Image;
    point.meta.valid = true;
    point.meta.score = std::min(a.meta.score, b.meta.score);
    results.append(point);
  };

  appendPoint(base + normal * height, 1);
  if (height > epsilon)
  {
    appendPoint(base - normal * height, 2);
  }

  return results;
}
}
