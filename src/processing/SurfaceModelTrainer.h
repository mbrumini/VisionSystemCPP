#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct SurfaceModelTrainingResult
{
  bool trained = false;
  std::vector<cv::Point> contour;
  cv::Mat templateImage;
  cv::Mat edgeImage;
  cv::Mat diagnosticImage;
};

class SurfaceModelTrainer
{
public:
  SurfaceModelTrainingResult trainFromRoi(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    int edgeSensitivity = 60,
    bool useConvexHull = false) const;
};
