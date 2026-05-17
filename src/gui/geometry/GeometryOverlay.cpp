#include "GeometryOverlay.h"

void GeometryOverlay::clear()
{
  points.clear();
  lines.clear();
  bands.clear();
}

bool GeometryOverlay::empty() const
{
  return points.empty() && lines.empty() && bands.empty();
}
