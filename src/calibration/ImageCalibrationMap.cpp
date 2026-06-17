#include "calibration/ImageCalibrationMap.h"

#include <cmath>

ImageCalibrationMap::ImageCalibrationMap(CameraCalibrationModel model)
  : m_model(std::move(model))
{
}

const CameraCalibrationModel& ImageCalibrationMap::model() const
{
  return m_model;
}

bool ImageCalibrationMap::isValid() const
{
  return m_model.valid;
}

QPointF ImageCalibrationMap::imageToWorldMm(const QPointF& imagePoint) const
{
  if (!isValid())
  {
    return {};
  }

  const double radians = -m_model.rotationDegrees * 3.14159265358979323846 / 180.0;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  const QPointF delta = imagePoint - m_model.originImagePoint;
  const double xPixels = delta.x() * c - delta.y() * s;
  const double yPixels = delta.x() * s + delta.y() * c;
  return QPointF(
    m_model.originWorldMm.x() + xPixels * m_model.pixelSizeXMm,
    m_model.originWorldMm.y() + yPixels * m_model.pixelSizeYMm);
}

QPointF ImageCalibrationMap::worldMmToImage(const QPointF& worldPointMm) const
{
  if (!isValid() || m_model.pixelSizeXMm <= 0.0 || m_model.pixelSizeYMm <= 0.0)
  {
    return {};
  }

  const double xPixels = (worldPointMm.x() - m_model.originWorldMm.x()) / m_model.pixelSizeXMm;
  const double yPixels = (worldPointMm.y() - m_model.originWorldMm.y()) / m_model.pixelSizeYMm;
  const double radians = m_model.rotationDegrees * 3.14159265358979323846 / 180.0;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return QPointF(
    m_model.originImagePoint.x() + xPixels * c - yPixels * s,
    m_model.originImagePoint.y() + xPixels * s + yPixels * c);
}

double ImageCalibrationMap::pixelsToMm(double pixels) const
{
  if (!isValid())
  {
    return 0.0;
  }

  const double pixelSize = (m_model.pixelSizeXMm + m_model.pixelSizeYMm) * 0.5;
  return pixels * pixelSize;
}
