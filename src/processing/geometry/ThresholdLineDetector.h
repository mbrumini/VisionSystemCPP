#pragma once

#include "geometry/LineGeometry.h"
#include "processing/geometry/GeometryDetectorCommon.h"

struct ThresholdLineDetectorConfig
{
  QString id;
  QString label;
  GeometryDetectorRoi roi;
  GeometryThresholdRange threshold;
  double minArea = 25.0;
};

struct ThresholdLineDetectorResult : GeometryDetectorResult
{
  LineGeometry line;
};

class ThresholdLineDetector
{
public:
  ThresholdLineDetectorResult detect(const cv::Mat& input, const ThresholdLineDetectorConfig& config) const;
};
