#pragma once

#include "geometry/GeometryCommon.h"

#include <vector>

struct LineGeometry
{
  GeometryMeta meta;
  cv::Point2d start;
  cv::Point2d end;
  std::vector<cv::Point2d> edgePoints;
};

double lineLength(const LineGeometry& line);
