#pragma once

#include <opencv2/core.hpp>

#include <vector>

struct LocalizationResult
{
  bool found = false;
  double backgroundLevel = 0.0;
  double thresholdValue = 0.0;
  double area = 0.0;
  cv::Point2d center;
  cv::Rect boundingRect;
  double angleRadians = 0.0;
  cv::Point2d xAxisStart;
  cv::Point2d xAxisEnd;
  cv::Point2d yAxisStart;
  cv::Point2d yAxisEnd;
  std::vector<cv::Point> contour;
  cv::Mat diagnosticImage;
  bool processed = false;
};

class LocalizationProcessor
{
public:
  LocalizationResult locateDarkObjectOnLightBackground(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    double thresholdFactor = 0.5,
    double thresholdOffset = 0.0,
    bool createDiagnosticImage = true) const;

private:
  double estimateBackgroundLevel(const cv::Mat& grayRoi) const;
};
