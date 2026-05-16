#include "SurfacePcaStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

SurfaceDefectResult SurfacePcaStrategy::locateByEdgePca(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity) const
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

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);

  const int high = std::clamp(edgeSensitivity, 1, 255);
  const int low = std::max(1, high / 2);
  cv::Mat edgeMask;
  cv::Canny(blurred, edgeMask, low, high);

  cv::Mat roiMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  roiMask(roi).setTo(255);
  cv::bitwise_and(edgeMask, roiMask, edgeMask);

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

  cv::rectangle(result.diagnosticImage, roi, cv::Scalar(255, 0, 0), 2);

  std::vector<cv::Point> edgePixels;
  cv::findNonZero(edgeMask, edgePixels);

  for (const std::vector<cv::Point>& contour : contours)
  {
    cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{contour}, 0, cv::Scalar(0, 255, 0), 2);
  }

  if (edgePixels.size() < 8)
  {
    result.processed = true;
    return result;
  }

  cv::Mat data(static_cast<int>(edgePixels.size()), 2, CV_64F);
  for (int i = 0; i < static_cast<int>(edgePixels.size()); ++i)
  {
    data.at<double>(i, 0) = edgePixels[i].x;
    data.at<double>(i, 1) = edgePixels[i].y;
  }

  const cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);
  const cv::Point2d center(pca.mean.at<double>(0, 0), pca.mean.at<double>(0, 1));
  const cv::Point2d xDirection(pca.eigenvectors.at<double>(0, 0), pca.eigenvectors.at<double>(0, 1));
  const cv::Point2d yDirection(-xDirection.y, xDirection.x);
  const double angle = std::atan2(xDirection.y, xDirection.x);

  const cv::Rect bounding = cv::boundingRect(edgePixels);
  const double axisLength = std::max(20.0, 0.55 * std::max(bounding.width, bounding.height));

  SurfaceBlob blob;
  blob.area = static_cast<double>(edgePixels.size());
  blob.center = center;
  blob.boundingRect = bounding;
  if (edgePixels.size() >= 3)
  {
    cv::convexHull(edgePixels, blob.contour);
  }

  result.totalArea = blob.area;
  result.blobs.push_back(blob);
  result.localization.found = true;
  result.localization.method = "edge_pca";
  result.localization.center = center;
  result.localization.angleRadians = angle;
  result.localization.score = std::min(1.0, static_cast<double>(edgePixels.size()) / static_cast<double>(std::max(1, roi.area()) / 20));
  result.localization.inputPoints = static_cast<int>(edgePixels.size());
  result.localization.usedPoints = static_cast<int>(edgePixels.size());
  result.localization.xAxisStart = center - xDirection * axisLength;
  result.localization.xAxisEnd = center + xDirection * axisLength;
  result.localization.yAxisStart = center - yDirection * axisLength;
  result.localization.yAxisEnd = center + yDirection * axisLength;

  cv::rectangle(result.diagnosticImage, bounding, cv::Scalar(255, 0, 0), 2);
  drawSurfaceCenterOfMass(result.diagnosticImage, center);
  cv::line(result.diagnosticImage, surfacePoint(result.localization.xAxisStart), surfacePoint(result.localization.xAxisEnd), cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
  cv::line(result.diagnosticImage, surfacePoint(result.localization.yAxisStart), surfacePoint(result.localization.yAxisEnd), cv::Scalar(255, 255, 0), 2, cv::LINE_AA);

  result.processed = true;
  return result;
}
