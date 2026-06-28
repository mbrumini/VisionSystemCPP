#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct CircleFitSettings
{
  double maxRadialError = 8.0;
  int minPoints = 8;
  bool robustRefit = true;
};

struct CircleFitResult
{
  bool found = false;
  cv::Point2d center;
  double radius = 0.0;
  double meanError = 0.0;
  int inputPoints = 0;
  int usedPoints = 0;
  std::vector<cv::Point> inliers;
};

class CircleFit
{
public:
  static CircleFitResult fit(const std::vector<cv::Point>& points, const CircleFitSettings& settings = {});
  static CircleFitResult fit(const std::vector<cv::Point2d>& points, const CircleFitSettings& settings = {});
};
