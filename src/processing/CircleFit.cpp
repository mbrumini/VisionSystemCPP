#include "CircleFit.h"

#include <cmath>

namespace
{
bool fitLeastSquares(const std::vector<cv::Point>& points, cv::Point2d& center, double& radius)
{
  if (points.size() < 3)
  {
    return false;
  }

  cv::Mat a(static_cast<int>(points.size()), 3, CV_64F);
  cv::Mat b(static_cast<int>(points.size()), 1, CV_64F);

  for (int i = 0; i < static_cast<int>(points.size()); ++i)
  {
    const double x = points[i].x;
    const double y = points[i].y;
    a.at<double>(i, 0) = x;
    a.at<double>(i, 1) = y;
    a.at<double>(i, 2) = 1.0;
    b.at<double>(i, 0) = -(x * x + y * y);
  }

  cv::Mat solution;
  if (!cv::solve(a, b, solution, cv::DECOMP_SVD))
  {
    return false;
  }

  const double circleA = solution.at<double>(0, 0);
  const double circleB = solution.at<double>(1, 0);
  const double circleC = solution.at<double>(2, 0);
  center = cv::Point2d(-circleA * 0.5, -circleB * 0.5);

  const double radiusSquared = center.x * center.x + center.y * center.y - circleC;
  if (radiusSquared <= 0.0)
  {
    return false;
  }

  radius = std::sqrt(radiusSquared);
  return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(radius);
}

double meanRadialError(const std::vector<cv::Point>& points, const cv::Point2d& center, double radius)
{
  if (points.empty())
  {
    return 0.0;
  }

  double total = 0.0;
  for (const cv::Point& point : points)
  {
    total += std::abs(cv::norm(cv::Point2d(point) - center) - radius);
  }

  return total / static_cast<double>(points.size());
}
}

CircleFitResult CircleFit::fit(const std::vector<cv::Point>& points, const CircleFitSettings& settings)
{
  CircleFitResult result;
  result.inputPoints = static_cast<int>(points.size());

  if (result.inputPoints < settings.minPoints)
  {
    return result;
  }

  cv::Point2d firstCenter;
  double firstRadius = 0.0;
  if (!fitLeastSquares(points, firstCenter, firstRadius))
  {
    return result;
  }

  result.center = firstCenter;
  result.radius = firstRadius;
  result.inliers = points;

  if (settings.robustRefit)
  {
    result.inliers.clear();
    const double maxError = std::max(0.0, settings.maxRadialError);

    for (const cv::Point& point : points)
    {
      const double radialError = std::abs(cv::norm(cv::Point2d(point) - firstCenter) - firstRadius);
      if (radialError <= maxError)
      {
        result.inliers.push_back(point);
      }
    }

    if (static_cast<int>(result.inliers.size()) >= settings.minPoints)
    {
      cv::Point2d refinedCenter;
      double refinedRadius = 0.0;
      if (fitLeastSquares(result.inliers, refinedCenter, refinedRadius))
      {
        result.center = refinedCenter;
        result.radius = refinedRadius;
      }
    }
    else
    {
      result.inliers = points;
    }
  }

  result.found = true;
  result.usedPoints = static_cast<int>(result.inliers.size());
  result.meanError = meanRadialError(result.inliers, result.center, result.radius);
  return result;
}
