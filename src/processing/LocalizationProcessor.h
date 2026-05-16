#pragma once

#include <opencv2/core.hpp>

struct LocalizationResult
{
  bool found = false;
  double backgroundLevel = 0.0;
  double thresholdValue = 0.0;
  double area = 0.0;
  cv::Point2d center;
  cv::Rect boundingRect;
  cv::Mat diagnosticImage;
};

class LocalizationProcessor
{
public:
  LocalizationResult locateDarkObjectOnLightBackground(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    double thresholdFactor = 0.5,
    double thresholdOffset = 0.0) const;

private:
  double estimateBackgroundLevel(const cv::Mat& grayRoi) const;
};
