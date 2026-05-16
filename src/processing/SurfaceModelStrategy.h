#pragma once

#include "processing/SurfaceDefectProcessor.h"

class SurfaceModelStrategy
{
public:
  SurfaceDefectResult locateByShapeMatching(
    const cv::Mat& input,
    const SurfaceShapeMatchConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;

  SurfaceDefectResult locateByTemplateMatching(
    const cv::Mat& input,
    const SurfaceTemplateMatchConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;
};
