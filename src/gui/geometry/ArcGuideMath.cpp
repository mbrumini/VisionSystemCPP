#include "gui/geometry/ArcGuideMath.h"

#include <algorithm>
#include <cmath>

namespace ArcGuideMath
{
double normalizedAngle(double angle)
{
  while (angle < 0.0)
  {
    angle += 2.0 * CV_PI;
  }
  while (angle >= 2.0 * CV_PI)
  {
    angle -= 2.0 * CV_PI;
  }
  return angle;
}

cv::Point2d pointOnArc(const cv::Point2d& center, double radius, double angle)
{
  return cv::Point2d(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
}

double arcMidAngle(double startAngle, double endAngle)
{
  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  return normalizedAngle(startAngle + span * 0.5);
}

void syncArcPartAngles(GeometryArcRuntimeConfig& arc)
{
  if (!arc.hasArc)
  {
    return;
  }

  arc.partStartAngleRadians =
    normalizedAngle(std::atan2(arc.partStart.y - arc.partCenter.y, arc.partStart.x - arc.partCenter.x));
  arc.partEndAngleRadians =
    normalizedAngle(std::atan2(arc.partEnd.y - arc.partCenter.y, arc.partEnd.x - arc.partCenter.x));
}

double partFrameAngleToImage(const PartPose& pose, double partAngleRadians)
{
  const cv::Point2d direction =
    pose.xAxis * std::cos(partAngleRadians) + pose.yAxis * std::sin(partAngleRadians);
  return normalizedAngle(std::atan2(direction.y, direction.x));
}

bool resolveArcGuide(const GeometryArcRuntimeConfig& arc, const PartPose& pose, ResolvedArcGuide& guide)
{
  const bool usePartArc = pose.valid && arc.hasArc && !arc.anchorInImageSpace;
  if (!usePartArc && !arc.hasImageArc)
  {
    return false;
  }

  guide.radius = std::max(1.0, arc.radius);

  if (usePartArc)
  {
    guide.center = partToImage(pose, arc.partCenter);
    guide.startAngle = partFrameAngleToImage(pose, arc.partStartAngleRadians);
    guide.endAngle = partFrameAngleToImage(pose, arc.partEndAngleRadians);
  }
  else
  {
    guide.center = arc.imageCenter;
    guide.startAngle = arc.startAngleRadians;
    guide.endAngle = arc.endAngleRadians;
  }

  guide.start = pointOnArc(guide.center, guide.radius, guide.startAngle);
  guide.end = pointOnArc(guide.center, guide.radius, guide.endAngle);
  return guide.radius > arc.innerBand;
}
}
