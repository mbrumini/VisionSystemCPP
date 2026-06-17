#include "geometry/ConstructedGeometryMath.h"

#include <algorithm>
#include <cmath>

namespace ConstructedGeometryMath
{
namespace
{
bool normalizedDirection(const LineGeometry& line, cv::Point2d& direction, double& length)
{
  direction = line.end - line.start;
  length = std::sqrt(direction.dot(direction));
  if (length < 0.000001)
  {
    return false;
  }

  direction *= 1.0 / length;
  return true;
}

bool normalizedVector(const cv::Point2d& value, cv::Point2d& result)
{
  const double length = std::sqrt(value.dot(value));
  if (length < 0.000001)
  {
    return false;
  }

  result = value * (1.0 / length);
  return true;
}
}

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

PointGeometry circleCenter(const CircleGeometry& circle)
{
  PointGeometry result;
  result.point = circle.center;
  result.meta.id = QString("%1_center").arg(circle.meta.id);
  result.meta.label = result.meta.id;
  result.meta.method = "constructed_circle_center";
  result.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.meta.valid = circle.meta.valid;
  result.meta.score = circle.meta.score;
  return result;
}

PointGeometry midpoint(const PointGeometry& a, const PointGeometry& b)
{
  PointGeometry result;
  result.point = (a.point + b.point) * 0.5;
  result.meta.id = QString("%1_mid_%2").arg(a.meta.id, b.meta.id);
  result.meta.label = result.meta.id;
  result.meta.method = "constructed_midpoint";
  result.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.meta.valid = a.meta.valid && b.meta.valid;
  result.meta.score = std::min(a.meta.score, b.meta.score);
  return result;
}

bool offsetLine(const LineGeometry& source, double offset, LineGeometry& result)
{
  cv::Point2d direction;
  double length = 0.0;
  if (!normalizedDirection(source, direction, length))
  {
    return false;
  }

  const cv::Point2d normal(-direction.y, direction.x);
  const cv::Point2d delta = normal * offset;
  result.start = source.start + delta;
  result.end = source.end + delta;
  result.meta.id = QString("%1_offset_%2").arg(source.meta.id).arg(offset, 0, 'f', 2);
  result.meta.label = result.meta.id;
  result.meta.method = "constructed_offset_line";
  result.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.meta.valid = source.meta.valid;
  result.meta.score = source.meta.score;
  return true;
}

QVector<LineGeometry> angleBisectors(const LineGeometry& a, const LineGeometry& b)
{
  QVector<LineGeometry> results;

  PointGeometry intersection;
  if (!lineLineIntersection(a, b, intersection))
  {
    return results;
  }

  cv::Point2d firstDirection;
  cv::Point2d secondDirection;
  double firstLength = 0.0;
  double secondLength = 0.0;
  if (!normalizedDirection(a, firstDirection, firstLength) ||
      !normalizedDirection(b, secondDirection, secondLength))
  {
    return results;
  }

  const double halfLength = std::max(25.0, (firstLength + secondLength) * 0.25);
  const auto appendBisector = [&](const cv::Point2d& rawDirection, int index) {
    cv::Point2d direction;
    if (!normalizedVector(rawDirection, direction))
    {
      return;
    }

    LineGeometry line;
    line.start = intersection.point - direction * halfLength;
    line.end = intersection.point + direction * halfLength;
    line.meta.id = QString("%1_bis_%2_%3").arg(a.meta.id, b.meta.id).arg(index);
    line.meta.label = line.meta.id;
    line.meta.method = "constructed_angle_bisector";
    line.meta.coordinateSpace = GeometryCoordinateSpace::Image;
    line.meta.valid = a.meta.valid && b.meta.valid;
    line.meta.score = std::min(a.meta.score, b.meta.score);
    results.append(line);
  };

  appendBisector(firstDirection + secondDirection, 1);
  appendBisector(firstDirection - secondDirection, 2);
  return results;
}

QVector<LineGeometry> tangentLinesFromPointToCircle(const PointGeometry& point, const CircleGeometry& circle)
{
  QVector<LineGeometry> results;
  if (circle.radius <= 0.0)
  {
    return results;
  }

  const cv::Point2d fromCenter = point.point - circle.center;
  const double distanceSquared = fromCenter.dot(fromCenter);
  const double radiusSquared = circle.radius * circle.radius;
  constexpr double epsilon = 0.000001;
  if (distanceSquared < radiusSquared - epsilon)
  {
    return results;
  }

  const auto appendLine = [&](const cv::Point2d& tangentPoint, int index) {
    LineGeometry line;
    line.start = point.point;
    line.end = tangentPoint;
    line.meta.id = QString("%1_tan_%2_%3").arg(point.meta.id, circle.meta.id).arg(index);
    line.meta.label = line.meta.id;
    line.meta.method = "constructed_tangent_line";
    line.meta.coordinateSpace = GeometryCoordinateSpace::Image;
    line.meta.valid = point.meta.valid && circle.meta.valid;
    line.meta.score = std::min(point.meta.score, circle.meta.score);
    results.append(line);
  };

  if (std::abs(distanceSquared - radiusSquared) <= epsilon)
  {
    cv::Point2d radiusDirection;
    if (!normalizedVector(fromCenter, radiusDirection))
    {
      return results;
    }

    const cv::Point2d tangentDirection(-radiusDirection.y, radiusDirection.x);
    const double halfLength = std::max(25.0, circle.radius);
    LineGeometry line;
    line.start = point.point - tangentDirection * halfLength;
    line.end = point.point + tangentDirection * halfLength;
    line.meta.id = QString("%1_tan_%2_1").arg(point.meta.id, circle.meta.id);
    line.meta.label = line.meta.id;
    line.meta.method = "constructed_tangent_line";
    line.meta.coordinateSpace = GeometryCoordinateSpace::Image;
    line.meta.valid = point.meta.valid && circle.meta.valid;
    line.meta.score = std::min(point.meta.score, circle.meta.score);
    results.append(line);
    return results;
  }

  const double distance = std::sqrt(distanceSquared);
  const cv::Point2d unit = fromCenter * (1.0 / distance);
  const cv::Point2d normal(-unit.y, unit.x);
  const double alongRadius = radiusSquared / distance;
  const double tangentHeight = circle.radius * std::sqrt(distanceSquared - radiusSquared) / distance;
  const cv::Point2d base = circle.center + unit * alongRadius;

  appendLine(base + normal * tangentHeight, 1);
  appendLine(base - normal * tangentHeight, 2);
  return results;
}

bool projectPointOntoLine(const PointGeometry& point, const LineGeometry& line, PointGeometry& result)
{
  const cv::Point2d direction = line.end - line.start;
  const double lengthSquared = direction.dot(direction);
  if (lengthSquared < 0.000001)
  {
    return false;
  }

  const double t = (point.point - line.start).dot(direction) / lengthSquared;
  result.point = line.start + direction * t;
  result.meta.id = QString("%1_proj_%2").arg(point.meta.id, line.meta.id);
  result.meta.label = result.meta.id;
  result.meta.method = "constructed_project_point_line";
  result.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.meta.valid = point.meta.valid && line.meta.valid;
  result.meta.score = std::min(point.meta.score, line.meta.score);
  return true;
}
}
