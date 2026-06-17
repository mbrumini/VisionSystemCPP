#pragma once

#include "calibration/CalibrationPatternDetector.h"
#include "calibration/PlanarCalibrationEstimator.h"

class CalibrationSolver
{
public:
  CameraCalibrationModel solvePlanarScale(const QString& cameraId,
                                          const cv::Mat& image,
                                          const CalibrationPatternConfig& pattern) const;
  CameraCalibrationModel solveFullCameraModel(const QString& cameraId,
                                              const QVector<CalibrationFrame>& frames,
                                              const CalibrationPatternConfig& pattern) const;

private:
  CalibrationPatternDetector m_detector;
  PlanarCalibrationEstimator m_planarEstimator;
};
