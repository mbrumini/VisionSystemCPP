#pragma once

#include "calibration/CalibrationTypes.h"

class ImageCalibrationMap
{
public:
  explicit ImageCalibrationMap(CameraCalibrationModel model = {});

  const CameraCalibrationModel& model() const;
  bool isValid() const;
  QPointF imageToWorldMm(const QPointF& imagePoint) const;
  QPointF worldMmToImage(const QPointF& worldPointMm) const;
  double pixelsToMm(double pixels) const;

private:
  CameraCalibrationModel m_model;
};
