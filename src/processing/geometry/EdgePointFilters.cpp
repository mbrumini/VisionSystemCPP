#include "EdgePointFilters.h"

#include <algorithm>
#include <cmath>

namespace
{
struct ProjectedEdgePoint
{
  cv::Point2d point;
  double along = 0.0;
  double normal = 0.0;
};

bool projectionAxes(
  const cv::Point2d& guideStart,
  const cv::Point2d& guideEnd,
  cv::Point2d& unit,
  cv::Point2d& normal)
{
  const cv::Point2d axis = guideEnd - guideStart;
  const double length = std::hypot(axis.x, axis.y);
  if (length <= 0.000001)
  {
    return false;
  }

  unit = axis * (1.0 / length);
  normal = cv::Point2d(-unit.y, unit.x);
  return true;
}

std::vector<ProjectedEdgePoint> projectedPoints(
  const std::vector<cv::Point2d>& points,
  const cv::Point2d& guideStart,
  const cv::Point2d& unit,
  const cv::Point2d& normal)
{
  std::vector<ProjectedEdgePoint> projected;
  projected.reserve(points.size());

  for (const cv::Point2d& point : points)
  {
    const cv::Point2d delta(point.x - guideStart.x, point.y - guideStart.y);
    projected.push_back({
      point,
      delta.x * unit.x + delta.y * unit.y,
      delta.x * normal.x + delta.y * normal.y
    });
  }

  return projected;
}

double median(std::vector<double> values)
{
  if (values.empty())
  {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  const int middle = static_cast<int>(values.size()) / 2;
  if ((values.size() % 2) == 1)
  {
    return values[middle];
  }

  return (values[middle - 1] + values[middle]) * 0.5;
}
}

std::vector<cv::Point2d> EdgePointFilters::filterByDerivative(
  const std::vector<cv::Point2d>& points,
  const EdgeDerivativeFilterConfig& config)
{
  if (points.size() < 3 || config.maxDerivative <= 0)
  {
    return points;
  }

  cv::Point2d unit;
  cv::Point2d normal;
  if (!projectionAxes(config.guideStart, config.guideEnd, unit, normal))
  {
    return points;
  }

  std::vector<ProjectedEdgePoint> projected = projectedPoints(points, config.guideStart, unit, normal);

  std::sort(projected.begin(), projected.end(), [](const ProjectedEdgePoint& a, const ProjectedEdgePoint& b) {
    return a.along < b.along;
  });

  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  filtered.push_back(projected.front().point);

  const double maxDerivative = static_cast<double>(config.maxDerivative);
  for (int i = 1; i < static_cast<int>(projected.size()) - 1; ++i)
  {
    const double previousJump = std::abs(projected[i].normal - projected[i - 1].normal);
    const double nextJump = std::abs(projected[i + 1].normal - projected[i].normal);
    if (std::min(previousJump, nextJump) <= maxDerivative)
    {
      filtered.push_back(projected[i].point);
    }
  }

  filtered.push_back(projected.back().point);
  return filtered;
}

std::vector<cv::Point2d> EdgePointFilters::filterByNormalMedianDeviation(
  const std::vector<cv::Point2d>& points,
  const EdgeStatisticalFilterConfig& config)
{
  if (points.size() < 3 || config.maxDeviation <= 0)
  {
    return points;
  }

  cv::Point2d unit;
  cv::Point2d normal;
  if (!projectionAxes(config.guideStart, config.guideEnd, unit, normal))
  {
    return points;
  }

  const std::vector<ProjectedEdgePoint> projected = projectedPoints(points, config.guideStart, unit, normal);
  std::vector<double> normalValues;
  normalValues.reserve(projected.size());
  for (const ProjectedEdgePoint& point : projected)
  {
    normalValues.push_back(point.normal);
  }

  const double center = median(normalValues);
  const double maxDeviation = static_cast<double>(config.maxDeviation);
  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  for (const ProjectedEdgePoint& point : projected)
  {
    if (std::abs(point.normal - center) <= maxDeviation)
    {
      filtered.push_back(point.point);
    }
  }

  return filtered;
}
