#include "calibration/CalibrationSolver.h"

CameraCalibrationModel CalibrationSolver::solvePlanarScale(const QString& cameraId,
                                                           const cv::Mat& image,
                                                           const CalibrationPatternConfig& pattern) const
{
  const QVector<QPointF> imagePoints = m_detector.detect(image, pattern);
  CameraCalibrationModel model = m_planarEstimator.estimateScaleOnly(cameraId, pattern, imagePoints);
  model.calibrationType = "checkerboard_scale";
  return model;
}

CameraCalibrationModel CalibrationSolver::solveFullCameraModel(const QString& cameraId,
                                                               const QVector<CalibrationFrame>& frames,
                                                               const CalibrationPatternConfig& pattern) const
{
  Q_UNUSED(frames);
  Q_UNUSED(pattern);
  CameraCalibrationModel model;
  model.cameraId = cameraId;
  model.calibrationType = "checkerboard_full";
  model.valid = false;
  return model;
}
