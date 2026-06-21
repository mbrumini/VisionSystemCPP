#pragma once

#include "processing/SurfaceDefectProcessor.h"

class SurfacePcaStrategy
{
public:
  SurfaceDefectResult locateByEdgePca(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    int edgeSensitivity = 60,
    bool resolveAmbiguity = false) const;

  SurfaceDefectResult locateByEdgePca(
    const cv::Mat& input,
    const std::vector<cv::Point>& searchPolygon,
    const std::vector<cv::Rect>& exclusionRects = {},
    int edgeSensitivity = 60,
    bool resolveAmbiguity = false) const;
};
