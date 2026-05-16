#include "LocalizationProcessor.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace
{
cv::Rect clampRect(const cv::Rect& rect, const cv::Size& imageSize)
{
  return rect & cv::Rect(0, 0, imageSize.width, imageSize.height);
}

cv::Rect cornerRect(int x, int y, int size)
{
  return cv::Rect(x, y, size, size);
}
}

LocalizationResult LocalizationProcessor::locateDarkObjectOnLightBackground(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  double thresholdFactor,
  double thresholdOffset) const
{
  LocalizationResult result;

  if (input.empty())
  {
    return result;
  }

  cv::Mat gray;

  if (input.channels() == 1)
  {
    gray = input;
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  }

  const cv::Rect roi = clampRect(searchRoi, gray.size());

  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  const cv::Mat grayRoi = gray(roi);
  result.backgroundLevel = estimateBackgroundLevel(grayRoi);
  result.thresholdValue = std::clamp(result.backgroundLevel * thresholdFactor + thresholdOffset, 0.0, 255.0);

  cv::Mat mask;
  cv::threshold(grayRoi, mask, result.thresholdValue, 255, cv::THRESH_BINARY_INV);

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  int bestIndex = -1;
  double bestArea = 0.0;

  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = cv::contourArea(contours[i]);

    if (area > bestArea)
    {
      bestArea = area;
      bestIndex = i;
    }
  }

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  cv::rectangle(result.diagnosticImage, roi, cv::Scalar(0, 255, 255), 2);

  if (bestIndex < 0)
  {
    return result;
  }

  std::vector<cv::Point> contour = contours[bestIndex];

  for (cv::Point& point : contour)
  {
    point.x += roi.x;
    point.y += roi.y;
  }

  const cv::Moments moments = cv::moments(contour);

  if (moments.m00 == 0.0)
  {
    return result;
  }

  result.found = true;
  result.area = bestArea;
  result.center = cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
  result.boundingRect = cv::boundingRect(contour);

  cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{contour}, 0, cv::Scalar(0, 255, 0), 2);
  cv::rectangle(result.diagnosticImage, result.boundingRect, cv::Scalar(255, 0, 0), 2);
  cv::circle(result.diagnosticImage, result.center, 8, cv::Scalar(0, 0, 255), -1);
  cv::line(
    result.diagnosticImage,
    cv::Point(static_cast<int>(result.center.x - 35), static_cast<int>(result.center.y)),
    cv::Point(static_cast<int>(result.center.x + 35), static_cast<int>(result.center.y)),
    cv::Scalar(0, 0, 255),
    2);
  cv::line(
    result.diagnosticImage,
    cv::Point(static_cast<int>(result.center.x), static_cast<int>(result.center.y - 35)),
    cv::Point(static_cast<int>(result.center.x), static_cast<int>(result.center.y + 35)),
    cv::Scalar(0, 0, 255),
    2);

  return result;
}

double LocalizationProcessor::estimateBackgroundLevel(const cv::Mat& grayRoi) const
{
  const int sampleSize = std::max(2, std::min(grayRoi.cols, grayRoi.rows) / 12);
  const int maxX = grayRoi.cols - sampleSize;
  const int maxY = grayRoi.rows - sampleSize;
  std::vector<double> samples;
  samples.reserve(4);

  samples.push_back(cv::mean(grayRoi(cornerRect(0, 0, sampleSize)))[0]);
  samples.push_back(cv::mean(grayRoi(cornerRect(maxX, 0, sampleSize)))[0]);
  samples.push_back(cv::mean(grayRoi(cornerRect(0, maxY, sampleSize)))[0]);
  samples.push_back(cv::mean(grayRoi(cornerRect(maxX, maxY, sampleSize)))[0]);

  std::sort(samples.begin(), samples.end());
  return (samples[1] + samples[2]) * 0.5;
}
