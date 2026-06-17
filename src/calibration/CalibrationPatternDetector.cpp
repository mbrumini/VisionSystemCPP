#include "calibration/CalibrationPatternDetector.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

QVector<QPointF> CalibrationPatternDetector::detect(const cv::Mat& image, const CalibrationPatternConfig& pattern) const
{
  QVector<QPointF> result;
  if (image.empty() || pattern.gridSize.width() <= 1 || pattern.gridSize.height() <= 1)
  {
    return result;
  }

  cv::Mat gray;
  if (image.channels() == 1)
  {
    gray = image;
  }
  else
  {
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  }

  std::vector<cv::Point2f> corners;
  const cv::Size patternSize(pattern.gridSize.width(), pattern.gridSize.height());
  bool found = false;

  if (pattern.type == CalibrationPatternType::Checkerboard)
  {
    found = cv::findChessboardCorners(
      gray,
      patternSize,
      corners,
      cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK);
    if (found)
    {
      cv::cornerSubPix(
        gray,
        corners,
        cv::Size(7, 7),
        cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.01));
    }
  }
  else if (pattern.type == CalibrationPatternType::CircleGrid || pattern.type == CalibrationPatternType::DotGrid)
  {
    found = cv::findCirclesGrid(gray, patternSize, corners);
  }

  if (!found)
  {
    return result;
  }

  result.reserve(static_cast<int>(corners.size()));
  for (const cv::Point2f& corner : corners)
  {
    result.append(QPointF(corner.x, corner.y));
  }
  return result;
}
