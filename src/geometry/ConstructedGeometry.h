#pragma once

#include "geometry/LineGeometry.h"
#include "geometry/PointGeometry.h"

#include <QString>

struct ConstructedPointGeometry
{
  PointGeometry point;
  QString sourceAId;
  QString sourceBId;
};

struct ConstructedLineGeometry
{
  LineGeometry line;
  QString sourceAId;
  QString sourceBId;
  double offset = 0.0;
};
