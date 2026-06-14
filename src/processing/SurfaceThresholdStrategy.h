#pragma once

#include "processing/SurfaceDefectProcessor.h"

class SurfaceThresholdStrategy
{
public:
  SurfaceDefectResult detectInRoi(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    const SurfaceThresholdSettings& settings = {}) const;

  SurfaceDefectResult detectInPolygon(
    const cv::Mat& input,
    const std::vector<cv::Point>& searchPolygon,
    const std::vector<cv::Rect>& exclusionRects = {},
    const SurfaceThresholdSettings& settings = {}) const;

  SurfaceDefectResult locateAnnulus(
    const cv::Mat& input,
    const SurfaceAnnulusThresholdConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;
};
