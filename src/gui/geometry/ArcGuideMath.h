#pragma once

#include "gui/geometry/GeometryRuntimeConfig.h"
#include "runtime/PartPose.h"

#include <opencv2/core.hpp>

struct ResolvedArcGuide
{
  cv::Point2d center;
  cv::Point2d start;
  cv::Point2d end;
  double radius = 0.0;
  double startAngle = 0.0;
  double endAngle = 0.0;
};

namespace ArcGuideMath
{
double normalizedAngle(double angle);
cv::Point2d pointOnArc(const cv::Point2d& center, double radius, double angle);
double arcMidAngle(double startAngle, double endAngle);
void syncArcPartAngles(GeometryArcRuntimeConfig& arc);
double partFrameAngleToImage(const PartPose& pose, double partAngleRadians);
bool resolveArcGuide(const GeometryArcRuntimeConfig& arc,
                     const PartPose& pose,
                     ResolvedArcGuide& guide,
                     const QSize& referenceSize = {},
                     const cv::Size& imageSize = {});
}
