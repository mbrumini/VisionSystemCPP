#include "LineGeometry.h"

#include <cmath>

double lineLength(const LineGeometry& line)
{
  const cv::Point2d vector = geometryVector(line.start, line.end);
  return std::hypot(vector.x, vector.y);
}
