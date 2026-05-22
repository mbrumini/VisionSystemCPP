#pragma once

#include <QString>

struct MeasurementResult
{
  QString id;
  QString type;
  QString sourceAId;
  QString sourceBId;
  double valuePixels = 0.0;
  bool valid = false;
};
