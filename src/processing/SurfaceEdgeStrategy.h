#pragma once

#include "processing/SurfaceDefectProcessor.h"

class SurfaceEdgeStrategy
{
public:
  SurfaceDefectResult locateAnnulus(
    const cv::Mat& input,
    const SurfaceAnnulusThresholdConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {},
    bool createDiagnosticImage = true,
    bool drawContours = true) const;
};
