#pragma once

#include "geometry/LineGeometry.h"
#include "processing/geometry/GeometryDetectorCommon.h"

enum class EdgeLineScanDirection
{
  NormalPositive,
  NormalNegative
};

enum class EdgeLineTransition
{
  DarkToLight,
  LightToDark
};

enum class EdgeLinePickMode
{
  First,
  Last,
  Best
};

struct EdgeLineDetectorConfig
{
  QString id;
  QString label;
  cv::Point2d guideStart;
  cv::Point2d guideEnd;
  int bandHalfWidth = 20;
  int edgeSensitivity = 60;
  int edgeCleanupDerivative = 12;
  int edgeStatisticalFilter = 0;
  int minPoints = 20;
  bool useSubpixel = false;
  EdgeLineScanDirection scanDirection = EdgeLineScanDirection::NormalPositive;
  EdgeLineTransition transition = EdgeLineTransition::LightToDark;
  EdgeLinePickMode pickMode = EdgeLinePickMode::First;
};

struct EdgeLineDetectorResult : GeometryDetectorResult
{
  LineGeometry line;
  std::vector<cv::Point2d> edgePoints;
  std::vector<cv::Point2d> rawEdgePoints;
  std::vector<cv::Point2d> derivativeEdgePoints;
  std::vector<cv::Point2d> statisticalEdgePoints;
  cv::Rect searchRoi;
};

class EdgeLineDetector
{
public:
  EdgeLineDetectorResult detect(const cv::Mat& input, const EdgeLineDetectorConfig& config) const;
};
