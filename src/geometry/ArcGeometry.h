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
