#include "calibration/CalibrationPatternDetector.h"

QVector<QPointF> CalibrationPatternDetector::detect(const cv::Mat& image, const CalibrationPatternConfig& pattern) const
{
  Q_UNUSED(image);
  Q_UNUSED(pattern);
  return {};
}
