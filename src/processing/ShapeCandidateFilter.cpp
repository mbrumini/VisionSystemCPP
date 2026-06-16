#include "ShapeCandidateFilter.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
double safeLogRatio(double reference, double value)
{
  if (reference <= 0.0 || value <= 0.0)
  {
    return 0.0;
  }

  return std::abs(std::log(value / reference));
}
}

ShapeCandidateMetrics measureShapeCandidate(const std::vector<cv::Point>& contour)
{
  ShapeCandidateMetrics metrics;
  if (contour.size() < 3)
  {
    return metrics;
  }

  metrics.area = std::abs(cv::contourArea(contour));
  const double perimeter = cv::arcLength(contour, true);
  metrics.boundingRect = cv::boundingRect(contour);
  const double boundingArea = static_cast<double>(metrics.boundingRect.area());
  const int minSide = std::min(metrics.boundingRect.width, metrics.boundingRect.height);
  const int maxSide = std::max(metrics.boundingRect.width, metrics.boundingRect.height);

  if (metrics.area <= 0.0 || perimeter <= 0.0 || boundingArea <= 0.0 || minSide <= 0)
  {
    return metrics;
  }

  metrics.circularity = std::clamp(4.0 * CV_PI * metrics.area / (perimeter * perimeter), 0.0, 1.0);
  metrics.aspectRatio = static_cast<double>(maxSide) / static_cast<double>(minSide);
  metrics.extent = std::clamp(metrics.area / boundingArea, 0.0, 1.0);
  metrics.valid = std::isfinite(metrics.circularity) &&
                  std::isfinite(metrics.aspectRatio) &&
                  std::isfinite(metrics.extent);
  return metrics;
}

double shapeCandidateStatisticDistance(
  const ShapeCandidateMetrics& model,
  const ShapeCandidateMetrics& candidate)
{
  if (!model.valid || !candidate.valid)
  {
    return std::numeric_limits<double>::max();
  }

  const double circularityReference = std::max(model.circularity, 0.20);
  const double circularityDistance = std::abs(candidate.circularity - model.circularity) / circularityReference;

  return 0.35 * safeLogRatio(model.area, candidate.area) +
         0.70 * safeLogRatio(model.aspectRatio, candidate.aspectRatio) +
         0.50 * safeLogRatio(model.extent, candidate.extent) +
         0.90 * circularityDistance;
}

bool acceptsShapeCandidate(
  const ShapeCandidateMetrics& model,
  const ShapeCandidateMetrics& candidate,
  double maxStatisticDistance)
{
  if (!model.valid || !candidate.valid)
  {
    return false;
  }

  const double allowedAspectRatio = model.aspectRatio < 1.35
                                      ? 2.20
                                      : std::max(2.20, model.aspectRatio * 2.10);
  if (candidate.aspectRatio > allowedAspectRatio)
  {
    return false;
  }

  if (model.circularity >= 0.55 && candidate.circularity < model.circularity * 0.52)
  {
    return false;
  }

  if (candidate.extent < std::max(0.03, model.extent * 0.38))
  {
    return false;
  }

  return shapeCandidateStatisticDistance(model, candidate) <= maxStatisticDistance;
}
