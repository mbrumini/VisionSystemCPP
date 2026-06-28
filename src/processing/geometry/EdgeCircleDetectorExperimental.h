#pragma once

#include "processing/geometry/EdgeCircleDetector.h"

// Official robust circle/arc edge detector. The class keeps its historical name
// so existing includes stay stable and the standard fallback remains available.
class EdgeCircleDetectorExperimental
{
public:
  EdgeCircleDetectorResult detect(const cv::Mat& input, const EdgeCircleDetectorConfig& config) const;
};

bool robustEdgeCircleDetectorEnabled();
EdgeCircleDetectorResult detectEdgeCircleWithSelectedDetector(const cv::Mat& input, const EdgeCircleDetectorConfig& config);
