#pragma once

#include "geometry/GeometryCommon.h"

#include <vector>

struct EdgeGeometry
{
  GeometryMeta meta;
  std::vector<cv::Point2d> points;
};
