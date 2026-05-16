#include "SurfaceEdgeStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

SurfaceDefectResult SurfaceEdgeStrategy::locateAnnulus(
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

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);

  const int high = std::clamp(config.edgeSensitivity, 1, 255);
  const int low = std::max(1, high / 2);
  cv::Mat edgeMask;
  cv::Canny(blurred, edgeMask, low, high);

  cv::Mat annulusMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  cv::circle(annulusMask, config.center, config.outerRadius, cv::Scalar(255), -1);
  cv::circle(annulusMask, config.center, config.innerRadius, cv::Scalar(0), -1);
  cv::bitwise_and(edgeMask, annulusMask, edgeMask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect maskExclusion = clampSurfaceRect(exclusionRect, edgeMask.size());

    if (!maskExclusion.empty())
    {
      edgeMask(maskExclusion).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(edgeMask, edgeMask, cv::MORPH_CLOSE, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edgeMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

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
