#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct EdgeDerivativeFilterConfig
{
  cv::Point2d guideStart;
  cv::Point2d guideEnd;
  int maxDerivative = 12;
};

struct EdgeStatisticalFilterConfig
{
  cv::Point2d guideStart;
  cv::Point2d guideEnd;
  int maxDeviation = 0;
};

class EdgePointFilters
{
public:
  static std::vector<cv::Point2d> filterByDerivative(
    const std::vector<cv::Point2d>& points,
    const EdgeDerivativeFilterConfig& config);

  static std::vector<cv::Point2d> filterByNormalMedianDeviation(
    const std::vector<cv::Point2d>& points,
    const EdgeStatisticalFilterConfig& config);
};
