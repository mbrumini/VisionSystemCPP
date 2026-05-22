#include "GeometryOverlay.h"

void GeometryOverlay::clear()
{
  points.clear();
  lines.clear();
  bands.clear();
  dimensions.clear();
  angles.clear();
}

bool GeometryOverlay::empty() const
{
  return points.empty() && lines.empty() && bands.empty() && dimensions.empty() && angles.empty();
}
