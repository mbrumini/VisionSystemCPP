#pragma once

#include "geometry/GeometryCommon.h"

struct CircleGeometry
{
  GeometryMeta meta;
  cv::Point2d center;
  double radius = 0.0;
  double meanError = 0.0;
};

double circleDiameter(const CircleGeometry& circle);
