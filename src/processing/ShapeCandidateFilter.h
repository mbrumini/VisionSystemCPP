#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct ShapeCandidateMetrics
{
  bool valid = false;
  double area = 0.0;
  double circularity = 0.0;
  double aspectRatio = 0.0;
  double extent = 0.0;
  cv::Rect boundingRect;
};

ShapeCandidateMetrics measureShapeCandidate(const std::vector<cv::Point>& contour);
double shapeCandidateStatisticDistance(
  const ShapeCandidateMetrics& model,
  const ShapeCandidateMetrics& candidate);
bool acceptsShapeCandidate(
  const ShapeCandidateMetrics& model,
  const ShapeCandidateMetrics& candidate,
  double maxStatisticDistance = 1.65);
