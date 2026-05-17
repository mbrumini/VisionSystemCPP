#pragma once

#include "geometry/GeometryCommon.h"

#include <vector>

struct ContourGeometry
{
  GeometryMeta meta;
  std::vector<cv::Point2d> points;
  double area = 0.0;
};
