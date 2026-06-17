#pragma once

#include <QString>
#include <QPointF>

struct MeasurementResult
{
  QString id;
  QString type;
  QString sourceAId;
  QString sourceBId;
  double valuePixels = 0.0;
  bool valid = false;
  QPointF labelPoint;
  bool hasLabelPoint = false;
  double valueReal = 0.0;
  QString unit = "px";
  bool hasRealValue = false;
  double nominal = 0.0;
  double min = 0.0;
  double max = 0.0;
  bool hasNominal = false;
  bool hasMin = false;
  bool hasMax = false;
  QString judgement;
};
