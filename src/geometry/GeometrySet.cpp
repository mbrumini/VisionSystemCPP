#include "GeometrySet.h"

void GeometrySet::clear()
{
  points.clear();
  lines.clear();
  circles.clear();
  arcs.clear();
  constructedPoints.clear();
  edges.clear();
  contours.clear();
}

bool GeometrySet::empty() const
{
  return points.empty() &&
    lines.empty() &&
    circles.empty() &&
    arcs.empty() &&
    constructedPoints.empty() &&
    edges.empty() &&
    contours.empty();
}
