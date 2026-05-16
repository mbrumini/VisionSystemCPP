#pragma once

#include "processing/SurfaceDefectProcessor.h"

class SurfaceTwoCirclesStrategy
{
public:
  SurfaceStrategyResult locate(
    const cv::Mat& input,
    const SurfaceTwoCirclesStrategyConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;
};
