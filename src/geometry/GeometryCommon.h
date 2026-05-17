#pragma once

#include <opencv2/core.hpp>

#include <QString>

enum class GeometryCoordinateSpace
{
  Image,
  Part
};

struct GeometryMeta
{
  QString id;
  QString label;
  QString method;
  GeometryCoordinateSpace coordinateSpace = GeometryCoordinateSpace::Part;
  bool valid = false;
  double score = 0.0;
};

inline cv::Point2d geometryVector(const cv::Point2d& from, const cv::Point2d& to)
{
  return {to.x - from.x, to.y - from.y};
}
