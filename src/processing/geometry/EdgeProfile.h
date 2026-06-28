#pragma once

#include "processing/geometry/EdgeLineDetector.h"

#include <opencv2/core.hpp>

#include <vector>

struct EdgeProfileSample
{
  cv::Point2d point;
  double value = 0.0;
};

struct EdgeProfileCandidate
{
  bool found = false;
  cv::Point2d point;
  double strength = 0.0;
  int index = -1;
};

class EdgeProfileSampler
{
public:
  static std::vector<EdgeProfileSample> sampleLine(
    const cv::Mat& gray,
    const cv::Point2d& start,
    const cv::Point2d& end,
    int steps);

  static double sampleBilinear(const cv::Mat& gray, const cv::Point2d& point);
  static bool pointInsideImage(const cv::Mat& image, const cv::Point2d& point);
};

class SubpixelEdgeLocator
{
public:
  static EdgeProfileCandidate locate(
    const std::vector<EdgeProfileSample>& profile,
    EdgeLineTransition transition,
    EdgeLinePickMode pickMode,
    int gradientThreshold,
    bool useSubpixel);
};
