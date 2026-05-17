#include "ThresholdMaskBuilder.h"

#include <opencv2/imgproc.hpp>

namespace
{
void applyGeometryExclusions(cv::Mat& mask, const std::vector<cv::Rect>& exclusionRects, const cv::Rect& roi)
{
  for (const cv::Rect& exclusion : exclusionRects)
  {
    const cv::Rect clipped = exclusion & roi;
    if (clipped.empty())
    {
      continue;
    }

    const cv::Rect local(clipped.x - roi.x, clipped.y - roi.y, clipped.width, clipped.height);
    mask(local).setTo(0);
  }
}
}

cv::Mat buildThresholdMask(
  const cv::Mat& input,
  const cv::Rect& roi,
  const GeometryThresholdRange& threshold,
  const std::vector<cv::Rect>& exclusionRects)
{
  cv::Mat gray;
  if (input.channels() == 1)
  {
    gray = input;
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  }

  cv::Mat roiGray = gray(roi);
  cv::Mat mask;
  cv::inRange(roiGray, cv::Scalar(threshold.minValue), cv::Scalar(threshold.maxValue), mask);

  if (threshold.polarity == GeometryPolarity::LightOnDark)
  {
    cv::bitwise_not(mask, mask);
  }

  applyGeometryExclusions(mask, exclusionRects, roi);
  return mask;
}
