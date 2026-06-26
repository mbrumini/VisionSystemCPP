#include "TestVisionMath.h"

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <numeric>

QVector<double> testVisionSteppedValues(double minimum, double maximum, double step)
{
  QVector<double> result;
  if (step <= 0.0 || maximum < minimum)
  {
    return result;
  }

  for (double value = minimum; value <= maximum + step * 0.0001; value += step)
  {
    result.append(value);
    if (result.size() > 100000)
    {
      break;
    }
  }
  return result;
}

double testVisionNormalizePcaAngleNear(double angle, double reference)
{
  while (angle - reference > 90.0)
  {
    angle -= 180.0;
  }
  while (angle - reference < -90.0)
  {
    angle += 180.0;
  }
  return angle;
}

double testVisionPcaAngleError(double actual, double expected)
{
  return std::abs(testVisionNormalizePcaAngleNear(actual, expected) - expected);
}

double testVisionStandardDeviation(const std::vector<double>& values)
{
  if (values.size() < 2)
  {
    return 0.0;
  }

  const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
  double sum = 0.0;
  for (double value : values)
  {
    const double delta = value - mean;
    sum += delta * delta;
  }
  return std::sqrt(sum / values.size());
}

double testVisionPearsonCorrelation(const QVector<double>& xs, const QVector<double>& ys)
{
  if (xs.size() != ys.size() || xs.size() < 2)
  {
    return 0.0;
  }

  double sumX = 0.0;
  double sumY = 0.0;
  for (int i = 0; i < xs.size(); ++i)
  {
    sumX += xs[i];
    sumY += ys[i];
  }
  const double meanX = sumX / xs.size();
  const double meanY = sumY / ys.size();

  double numerator = 0.0;
  double denomX = 0.0;
  double denomY = 0.0;
  for (int i = 0; i < xs.size(); ++i)
  {
    const double dx = xs[i] - meanX;
    const double dy = ys[i] - meanY;
    numerator += dx * dy;
    denomX += dx * dx;
    denomY += dy * dy;
  }
  if (denomX <= 0.0 || denomY <= 0.0)
  {
    return 0.0;
  }
  return numerator / std::sqrt(denomX * denomY);
}

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
  double pxPerMm)
{
  const double deltaAngleRad =
    (currentAngleDeg - baselineAngleDeg) * std::acos(-1.0) / 180.0;
  const double tx0 = baselineTxMm * pxPerMm;
  const double ty0 = baselineTyMm * pxPerMm;
  const double tx = currentTxMm * pxPerMm;
  const double ty = currentTyMm * pxPerMm;
  const double vx = baselineX - imageCenterX - tx0;
  const double vy = baselineY - imageCenterY - ty0;
  const double cosA = std::cos(deltaAngleRad);
  const double sinA = std::sin(deltaAngleRad);
  return QPointF(
    imageCenterX + cosA * vx - sinA * vy + tx,
    imageCenterY + sinA * vx + cosA * vy + ty);
}
