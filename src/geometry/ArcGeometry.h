#pragma once

#include "geometry/GeometryCommon.h"

struct ArcGeometry
{
  GeometryMeta meta;
  cv::Point2d center;
  double radius = 0.0;
  double startAngleRadians = 0.0;
  double endAngleRadians = 0.0;
  double meanError = 0.0;
};

double arcSpanRadians(const ArcGeometry& arc);
double arcNormalizedAngle(double angleRadians);
cv::Point2d arcPointAt(const cv::Point2d& center, double radius, double angleRadians);
bool arcAngleOnSpan(double startAngleRadians, double endAngleRadians, double angleRadians);
cv::Point2d arcStartPoint(const ArcGeometry& arc);
cv::Point2d arcEndPoint(const ArcGeometry& arc);
cv::Point2d closestPointOnArc(const ArcGeometry& arc, const cv::Point2d& point);
