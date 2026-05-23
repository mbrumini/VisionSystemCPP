#include "calibration/ImageCalibrationMap.h"

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
  Q_UNUSED(imagePoint);
  return {};
}

QPointF ImageCalibrationMap::worldMmToImage(const QPointF& worldPointMm) const
{
  Q_UNUSED(worldPointMm);
  return {};
}

double ImageCalibrationMap::pixelsToMm(double pixels) const
{
  Q_UNUSED(pixels);
  return 0.0;
}
