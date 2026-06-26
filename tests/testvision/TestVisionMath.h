#pragma once

#include <QPointF>
#include <QVector>

#include <vector>

QVector<double> testVisionSteppedValues(double minimum, double maximum, double step);
double testVisionNormalizePcaAngleNear(double angle, double reference);
double testVisionPcaAngleError(double actual, double expected);
double testVisionStandardDeviation(const std::vector<double>& values);
double testVisionPearsonCorrelation(const QVector<double>& xs, const QVector<double>& ys);

// Expected image-space pose after the same rotation/translation applied to the master image.
QPointF testVisionExpectedImageCenter(
  double baselineX,
  double baselineY,
  double baselineAngleDeg,
  double baselineTxMm,
  double baselineTyMm,
  double currentAngleDeg,
  double currentTxMm,
  double currentTyMm,
  double imageCenterX,
  double imageCenterY,
  double pxPerMm);
