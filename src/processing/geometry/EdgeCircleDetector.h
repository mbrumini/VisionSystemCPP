#pragma once

#include "geometry/ArcGeometry.h"
#include "geometry/CircleGeometry.h"
#include "processing/geometry/EdgeLineDetector.h"
#include "processing/geometry/GeometryDetectorCommon.h"

struct EdgeCircleDetectorConfig
{
  QString id;
  QString label;
  cv::Point2d guideCenter;
  double guideRadius = 0.0;
  int innerBand = 20;
  int outerBand = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  int minPoints = 30;
  bool useArc = false;
  double startAngleRadians = 0.0;
  double endAngleRadians = 0.0;
  bool useSubpixel = false;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
};

struct EdgeCircleDetectorResult : GeometryDetectorResult
{
  CircleGeometry circle;
  ArcGeometry arc;
  std::vector<cv::Point2d> edgePoints;
  std::vector<cv::Point2d> rawEdgePoints;
};

class EdgeCircleDetector
{
public:
  EdgeCircleDetectorResult detect(const cv::Mat& input, const EdgeCircleDetectorConfig& config) const;
};
