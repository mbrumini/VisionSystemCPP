#include "SurfaceModelTrainer.h"

#include "processing/SurfaceProcessingUtils.h"

#include <opencv2/imgproc.hpp>

SurfaceModelTrainingResult SurfaceModelTrainer::trainFromRoi(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity,
  bool useConvexHull) const
{
  SurfaceModelTrainingResult result;

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

  cv::Mat edges;
  cv::Canny(blurred, edges, low, high);

  cv::Mat roiMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  roiMask(roi).setTo(255);
  cv::bitwise_and(edges, roiMask, edges);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clamped = clampSurfaceRect(exclusionRect, edges.size());
    if (!clamped.empty())
    {
      edges(clamped).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
  cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);
  cv::dilate(edges, edges, kernel);
  result.edgeImage = edges(roi).clone();

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  int bestIndex = -1;
  double bestArea = 0.0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = std::abs(cv::contourArea(contours[i]));
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
  cv::rectangle(result.diagnosticImage, roi, cv::Scalar(255, 0, 0), 2);

  std::vector<cv::Point> modelContour;
  const double minModelArea = std::max(50.0, static_cast<double>(roi.area()) * 0.01);

  if (useConvexHull)
  {
    std::vector<cv::Point> hullPoints;
    for (const auto& c : contours)
    {
      if (std::abs(cv::contourArea(c)) >= 10.0)
      {
        hullPoints.insert(hullPoints.end(), c.begin(), c.end());
      }
    }
    if (hullPoints.empty())
    {
      cv::findNonZero(edges, hullPoints);
    }
    if (hullPoints.size() >= 3)
    {
      cv::convexHull(hullPoints, modelContour);
    }
  }
  else if (bestIndex >= 0 && bestArea >= minModelArea)
  {
    modelContour = contours[bestIndex];
  }
  else
  {
    std::vector<cv::Point> edgePixels;
    cv::findNonZero(edges, edgePixels);
    if (edgePixels.size() >= 3)
    {
      cv::convexHull(edgePixels, modelContour);
    }
  }

  if (modelContour.size() < 3 || std::abs(cv::contourArea(modelContour)) < minModelArea)
  {
    return result;
  }

  result.contour = modelContour;
  result.templateImage = input(roi).clone();
  cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{result.contour}, 0, cv::Scalar(0, 255, 0), 2);
  result.trained = true;
  return result;
}

SurfaceModelTrainingResult SurfaceModelTrainer::trainFromRotatedRoi(
  const cv::Mat& input,
  const cv::RotatedRect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity,
  bool useConvexHull) const
{
  SurfaceModelTrainingResult result;
  if (input.empty() || searchRoi.size.width < 4.0F || searchRoi.size.height < 4.0F)
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
  const int high = std::clamp(edgeSensitivity, 1, 255);
  const int low = std::max(1, high / 2);

  cv::Mat edges;
  cv::Canny(blurred, edges, low, high);

  cv::Mat roiMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  cv::Point2f box[4];
  searchRoi.points(box);
  const std::vector<cv::Point> polygon = {
    box[0],
    box[1],
    box[2],
    box[3]
  };
  cv::fillConvexPoly(roiMask, polygon, cv::Scalar(255));
  cv::bitwise_and(edges, roiMask, edges);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clamped = clampSurfaceRect(exclusionRect, edges.size());
    if (!clamped.empty())
    {
      edges(clamped).setTo(0);
    }
  }

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
  cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);
  cv::dilate(edges, edges, kernel);

  const cv::Rect roiBounds = searchRoi.boundingRect();
  result.edgeImage = edges(roiBounds).clone();

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  int bestIndex = -1;
  double bestArea = 0.0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = std::abs(cv::contourArea(contours[i]));
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
  cv::polylines(
    result.diagnosticImage,
    polygon,
    true,
    cv::Scalar(255, 0, 0),
    2,
    cv::LINE_AA);

  const double minModelArea = std::max(50.0, static_cast<double>(searchRoi.size.area()) * 0.01);
  std::vector<cv::Point> modelContour;
  if (useConvexHull)
  {
    std::vector<cv::Point> hullPoints;
    for (const auto& contour : contours)
    {
      if (std::abs(cv::contourArea(contour)) >= 10.0)
      {
        hullPoints.insert(hullPoints.end(), contour.begin(), contour.end());
      }
    }
    if (hullPoints.empty())
    {
      cv::findNonZero(edges, hullPoints);
    }
    if (hullPoints.size() >= 3)
    {
      cv::convexHull(hullPoints, modelContour);
    }
  }
  else if (bestIndex >= 0 && bestArea >= minModelArea)
  {
    modelContour = contours[bestIndex];
  }
  else
  {
    std::vector<cv::Point> edgePixels;
    cv::findNonZero(edges, edgePixels);
    if (edgePixels.size() >= 3)
    {
      cv::convexHull(edgePixels, modelContour);
    }
  }

  if (modelContour.size() < 3 || std::abs(cv::contourArea(modelContour)) < minModelArea)
  {
    return result;
  }

  cv::Mat rotation = cv::getRotationMatrix2D(searchRoi.center, searchRoi.angle, 1.0);
  cv::Mat rotated;
  cv::warpAffine(input, rotated, rotation, input.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT);
  cv::getRectSubPix(
    rotated,
    searchRoi.size,
    searchRoi.center,
    result.templateImage);

  result.contour = modelContour;
  cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{result.contour}, 0, cv::Scalar(0, 255, 0), 2);
  result.trained = true;
  return result;
}
