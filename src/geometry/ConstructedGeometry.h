#pragma once

#include "geometry/PointGeometry.h"

#include <QString>

struct ConstructedPointGeometry
{
  PointGeometry point;
  QString sourceAId;
  QString sourceBId;
};
