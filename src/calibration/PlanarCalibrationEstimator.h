#pragma once

#include "calibration/CalibrationTypes.h"

class PlanarCalibrationEstimator
{
public:
  CameraCalibrationModel estimateScaleOnly(const QString& cameraId,
                                           const CalibrationPatternConfig& pattern,
                                           const QVector<QPointF>& imagePoints) const;
};
