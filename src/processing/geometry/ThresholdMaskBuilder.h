#pragma once

#include "processing/geometry/GeometryDetectorCommon.h"

#include <opencv2/core.hpp>

cv::Mat buildThresholdMask(
  const cv::Mat& input,
  const cv::Rect& roi,
  const GeometryThresholdRange& threshold,
  const std::vector<cv::Rect>& exclusionRects);
