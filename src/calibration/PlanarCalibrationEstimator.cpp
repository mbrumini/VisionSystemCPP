#include "calibration/PlanarCalibrationEstimator.h"

#include <cmath>

namespace
{
double distance(const QPointF& a, const QPointF& b)
{
  const QPointF delta = a - b;
  return std::hypot(delta.x(), delta.y());
}

double angleDegrees(const QPointF& a, const QPointF& b)
{
  const QPointF delta = b - a;
  return std::atan2(delta.y(), delta.x()) * 180.0 / 3.14159265358979323846;
}
}

CameraCalibrationModel PlanarCalibrationEstimator::estimateScaleOnly(const QString& cameraId,
                                                                     const CalibrationPatternConfig& pattern,
                                                                     const QVector<QPointF>& imagePoints) const
{
  CameraCalibrationModel model;
  model.cameraId = cameraId;
  model.modelId = QString("%1_planar_scale").arg(cameraId);
  model.calibrationType = "checkerboard_scale";

  const int columns = pattern.gridSize.width();
  const int rows = pattern.gridSize.height();
  const int expectedPoints = columns * rows;
  if (columns <= 1 || rows <= 1 || pattern.pitchMm <= 0.0 || imagePoints.size() < expectedPoints)
  {
    return model;
  }

  double horizontalPixels = 0.0;
  int horizontalCount = 0;
  for (int row = 0; row < rows; ++row)
  {
    for (int column = 0; column < columns - 1; ++column)
    {
      const int index = row * columns + column;
      horizontalPixels += distance(imagePoints[index], imagePoints[index + 1]);
      ++horizontalCount;
    }
  }

  double verticalPixels = 0.0;
  int verticalCount = 0;
  for (int row = 0; row < rows - 1; ++row)
  {
    for (int column = 0; column < columns; ++column)
    {
      const int index = row * columns + column;
      verticalPixels += distance(imagePoints[index], imagePoints[index + columns]);
      ++verticalCount;
    }
  }

  if (horizontalCount <= 0 || verticalCount <= 0)
  {
    return model;
  }

  const double avgHorizontalPixels = horizontalPixels / horizontalCount;
  const double avgVerticalPixels = verticalPixels / verticalCount;
  if (avgHorizontalPixels <= 0.000001 || avgVerticalPixels <= 0.000001)
  {
    return model;
  }

  model.pixelSizeXMm = pattern.pitchMm / avgHorizontalPixels;
  model.pixelSizeYMm = pattern.pitchMm / avgVerticalPixels;
  model.originImagePoint = imagePoints.first();
  model.originWorldMm = QPointF(0.0, 0.0);
  model.rotationDegrees = angleDegrees(imagePoints.first(), imagePoints[1]);
  model.valid = true;

  double squaredError = 0.0;
  int errorCount = 0;
  for (int row = 0; row < rows; ++row)
  {
    for (int column = 0; column < columns - 1; ++column)
    {
      const int index = row * columns + column;
      const double pixels = distance(imagePoints[index], imagePoints[index + 1]);
      squaredError += std::pow(pixels - avgHorizontalPixels, 2.0);
      ++errorCount;
    }
  }
  for (int row = 0; row < rows - 1; ++row)
  {
    for (int column = 0; column < columns; ++column)
    {
      const int index = row * columns + column;
      const double pixels = distance(imagePoints[index], imagePoints[index + columns]);
      squaredError += std::pow(pixels - avgVerticalPixels, 2.0);
      ++errorCount;
    }
  }
  model.rmsErrorPixels = errorCount > 0 ? std::sqrt(squaredError / errorCount) : 0.0;
  return model;
}
