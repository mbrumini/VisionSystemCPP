#include "ArcGeometry.h"

#include <cmath>

double arcSpanRadians(const ArcGeometry& arc)
{
  double span = arc.endAngleRadians - arc.startAngleRadians;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  return span;
}
