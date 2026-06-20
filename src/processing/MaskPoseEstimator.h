#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct MaskPoseResult
{
  bool found = false;
  cv::Point2d center;
  double angleRadians = 0.0;
  double area = 0.0;
  cv::Rect boundingRect;
  std::vector<cv::Point> contour;
};

class MaskPoseEstimator
{
public:
  MaskPoseResult estimate(const cv::Mat& mask, double minArea = 25.0) const;
};
