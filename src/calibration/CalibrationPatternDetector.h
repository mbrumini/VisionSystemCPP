#pragma once

#include "calibration/CalibrationTypes.h"

#include <opencv2/core/mat.hpp>

class CalibrationPatternDetector
{
public:
  QVector<QPointF> detect(const cv::Mat& image, const CalibrationPatternConfig& pattern) const;
};
