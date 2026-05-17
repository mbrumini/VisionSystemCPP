#pragma once

#include "geometry/GeometryCommon.h"

struct LineGeometry
{
  GeometryMeta meta;
  cv::Point2d start;
  cv::Point2d end;
};

double lineLength(const LineGeometry& line);
