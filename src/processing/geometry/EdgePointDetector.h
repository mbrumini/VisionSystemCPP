#pragma once

#include "geometry/PointGeometry.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/GeometryDetectorCommon.h"

struct EdgePointDetectorConfig
{
  QString id;
  QString label;
  cv::Point2d scanStart;
  cv::Point2d scanEnd;
  int edgeSensitivity = 60;
  bool useSubpixel = false;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
};

struct EdgePointDetectorResult : GeometryDetectorResult
{
  PointGeometry point;
  std::vector<cv::Point2d> candidates;
};

class EdgePointDetector
{
public:
  EdgePointDetectorResult detect(const cv::Mat& input, const EdgePointDetectorConfig& config) const;
};
