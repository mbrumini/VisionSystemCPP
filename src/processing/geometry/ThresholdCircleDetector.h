#pragma once

#include "geometry/CircleGeometry.h"
#include "processing/geometry/GeometryDetectorCommon.h"

struct ThresholdCircleDetectorConfig
{
  QString id;
  QString label;
  GeometryDetectorRoi roi;
  GeometryThresholdRange threshold;
  double minArea = 25.0;
  double maxArea = 0.0;
};

struct ThresholdCircleDetectorResult : GeometryDetectorResult
{
  CircleGeometry circle;
};

class ThresholdCircleDetector
{
public:
  ThresholdCircleDetectorResult detect(const cv::Mat& input, const ThresholdCircleDetectorConfig& config) const;
};
