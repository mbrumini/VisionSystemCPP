#include "GeometryMath.h"

#include <QtGlobal>

#include <cmath>

namespace GeometryMath
{
bool circleFromThreePoints(const QVector<QPoint>& points, ImageCircle& circle)
{
  if (points.size() < 3)
  {
    return false;
  }

  const double x1 = points[0].x();
  const double y1 = points[0].y();
  const double x2 = points[1].x();
  const double y2 = points[1].y();
  const double x3 = points[2].x();
  const double y3 = points[2].y();
  const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));

  if (std::abs(d) < 0.001)
  {
    return false;
  }

  const double ux = ((x1 * x1 + y1 * y1) * (y2 - y3) +
                     (x2 * x2 + y2 * y2) * (y3 - y1) +
                     (x3 * x3 + y3 * y3) * (y1 - y2)) / d;
  const double uy = ((x1 * x1 + y1 * y1) * (x3 - x2) +
                     (x2 * x2 + y2 * y2) * (x1 - x3) +
                     (x3 * x3 + y3 * y3) * (x2 - x1)) / d;
  const QPoint center(qRound(ux), qRound(uy));
  const int radius = qRound(std::hypot(x1 - ux, y1 - uy));

  if (radius <= 2)
  {
    return false;
  }

  circle = {center, radius};
  return true;
}
}
