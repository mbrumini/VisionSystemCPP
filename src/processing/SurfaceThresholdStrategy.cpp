#include "SurfaceThresholdStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

SurfaceDefectResult SurfaceThresholdStrategy::detectInRoi(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings) const
{
  SurfaceDefectResult result;

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

  const cv::Rect roi = clampSurfaceRect(searchRoi, gray.size());

  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  const int minValue = std::clamp(settings.minValue, 0, 255);
  const int maxValue = std::clamp(settings.maxValue, minValue, 255);
  const cv::Mat grayRoi = gray(roi);

  cv::Mat mask;
  cv::inRange(grayRoi, cv::Scalar(minValue), cv::Scalar(maxValue), mask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clampedExclusion = clampSurfaceRect(exclusionRect, gray.size());
    const cv::Rect localExclusion(
      clampedExclusion.x - roi.x,
      clampedExclusion.y - roi.y,
      clampedExclusion.width,
      clampedExclusion.height);
    const cv::Rect maskExclusion = clampSurfaceRect(localExclusion, mask.size());

    if (!maskExclusion.empty())
    {
      mask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  cv::rectangle(result.diagnosticImage, roi, cv::Scalar(0, 255, 255), 2);

  for (std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);

    if (area <= 0.0)
    {
      continue;
    }

    for (cv::Point& point : contour)
    {
      point.x += roi.x;
      point.y += roi.y;
    }

    const cv::Moments moments = cv::moments(contour);
    SurfaceBlob blob;
    blob.area = area;
    blob.center = moments.m00 == 0.0 ? cv::Point2d() : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    blob.boundingRect = cv::boundingRect(contour);
    blob.contour = contour;
    result.totalArea += area;
    result.blobs.push_back(blob);
  }

  sortSurfaceBlobsByArea(result);
  drawSurfaceBlobs(result.diagnosticImage, result.blobs);

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfaceThresholdStrategy::locateAnnulus(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  SurfaceDefectResult result;

  if (input.empty() || config.outerRadius <= 0 || config.innerRadius < 0 || config.innerRadius >= config.outerRadius)
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

  cv::Mat thresholdMask;
  const int minValue = std::clamp(config.threshold.minValue, 0, 255);
  const int maxValue = std::clamp(config.threshold.maxValue, minValue, 255);
  cv::inRange(gray, cv::Scalar(minValue), cv::Scalar(maxValue), thresholdMask);

  cv::Mat annulusMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  cv::circle(annulusMask, config.center, config.outerRadius, cv::Scalar(255), -1);
  cv::circle(annulusMask, config.center, config.innerRadius, cv::Scalar(0), -1);
  cv::bitwise_and(thresholdMask, annulusMask, thresholdMask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect maskExclusion = clampSurfaceRect(exclusionRect, thresholdMask.size());

    if (!maskExclusion.empty())
    {
      thresholdMask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(thresholdMask, thresholdMask, cv::MORPH_OPEN, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(thresholdMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  cv::circle(result.diagnosticImage, config.center, config.outerRadius, cv::Scalar(0, 255, 0), 2);
  cv::circle(result.diagnosticImage, config.center, config.innerRadius, cv::Scalar(0, 165, 255), 2);

  for (std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);

    if (area <= 0.0)
    {
      continue;
    }

    const cv::Moments moments = cv::moments(contour);
    SurfaceBlob blob;
    blob.area = area;
    blob.center = moments.m00 == 0.0 ? cv::Point2d() : cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
    blob.boundingRect = cv::boundingRect(contour);
    blob.contour = contour;
    result.totalArea += area;
    result.blobs.push_back(blob);
  }

  sortSurfaceBlobsByArea(result);
  drawSurfaceBlobs(result.diagnosticImage, result.blobs);

  result.processed = true;
  return result;
}
