#pragma once

#include "processing/geometry/EdgeCircleDetector.h"

// Experimental circle-edge detector kept separate from EdgeCircleDetector so the
// production path can be switched back instantly if the new strategy is weaker.
class EdgeCircleDetectorExperimental
{
public:
  EdgeCircleDetectorResult detect(const cv::Mat& input, const EdgeCircleDetectorConfig& config) const;
};

bool robustEdgeCircleDetectorEnabled();
EdgeCircleDetectorResult detectEdgeCircleWithSelectedDetector(const cv::Mat& input, const EdgeCircleDetectorConfig& config);
