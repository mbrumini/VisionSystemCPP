#include "SurfacePcaStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

namespace
{
void fillPolygonMask(cv::Mat& mask, const std::vector<cv::Point>& polygon)
{
  if (polygon.size() < 3)
  {
    return;
  }

  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{polygon}, cv::Scalar(255));
}
}

SurfaceDefectResult SurfacePcaStrategy::locateByEdgePca(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity,
  bool resolveAmbiguity) const
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
  cv::findContours(edgeMask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::rectangle(result.diagnosticImage, roi, cv::Scalar(255, 0, 0), 2);

  // Determine if there is at least one significant contour that does not span the entire ROI
  bool hasSmallSignificantContour = false;
  for (const auto& c : contours)
  {
    if (c.size() >= 30)
    {
      const cv::Rect bounding = cv::boundingRect(c);
      if (bounding.width <= 0.95 * roi.width || bounding.height <= 0.95 * roi.height)
      {
        hasSmallSignificantContour = true;
        break;
      }
    }
  }

  int bestContourIndex = -1;
  size_t bestContourPoints = 0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    if (contours[i].size() < 8)
    {
      continue;
    }

    if (hasSmallSignificantContour)
    {
      const cv::Rect bounding = cv::boundingRect(contours[i]);
      if (bounding.width > 0.95 * roi.width && bounding.height > 0.95 * roi.height)
      {
        continue;
      }
    }

    if (contours[i].size() > bestContourPoints)
    {
      bestContourIndex = i;
      bestContourPoints = contours[i].size();
    }
  }

  double bestContourArea = 0.0;
  if (bestContourIndex >= 0)
  {
    bestContourArea = std::abs(cv::contourArea(contours[bestContourIndex]));
  }

  std::vector<cv::Point> edgePixels;
  if (bestContourIndex >= 0)
  {
    edgePixels = smoothSurfaceContour(contours[bestContourIndex], 5);
    drawStyledContour(result.diagnosticImage, edgePixels, cv::Scalar(94, 197, 34));
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
  double angle = std::atan2(xDirection.y, xDirection.x);

  if (resolveAmbiguity && bestContourIndex >= 0)
  {
    const std::vector<std::vector<cv::Point>>& allContours = contours;

    int bestInternalIndex = -1;
    double bestInternalArea = 0.0;
    for (int i = 0; i < static_cast<int>(allContours.size()); ++i)
    {
      const double area = std::abs(cv::contourArea(allContours[i]));
      if (area < 0.8 * bestContourArea && area > 8.0)
      {
        if (area > bestInternalArea)
        {
          bestInternalIndex = i;
          bestInternalArea = area;
        }
      }
    }

    if (bestInternalIndex >= 0)
    {
      cv::Moments m = cv::moments(allContours[bestInternalIndex]);
      cv::Point2d internalCenter;
      if (std::abs(m.m00) > 1e-6)
      {
        internalCenter = cv::Point2d(m.m10 / m.m00, m.m01 / m.m00);
      }
      else
      {
        cv::Rect r = cv::boundingRect(allContours[bestInternalIndex]);
        internalCenter = cv::Point2d(r.x + r.width / 2.0, r.y + r.height / 2.0);
      }

      cv::drawContours(result.diagnosticImage, allContours, bestInternalIndex, cv::Scalar(0, 165, 255), 2);

      const cv::Point2d toInternal = internalCenter - center;
      const double dot = toInternal.x * xDirection.x + toInternal.y * xDirection.y;
      if (dot < 0.0)
      {
        angle = angle + CV_PI;
        if (angle > CV_PI)
        {
          angle -= 2.0 * CV_PI;
        }
        else if (angle <= -CV_PI)
        {
          angle += 2.0 * CV_PI;
        }
      }
    }
  }

  const cv::Point2d xDir(std::cos(angle), std::sin(angle));
  const cv::Point2d yDir(-xDir.y, xDir.x);

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
  result.localization.xAxisStart = center - xDir * axisLength;
  result.localization.xAxisEnd = center + xDir * axisLength;
  result.localization.yAxisStart = center - yDir * axisLength;
  result.localization.yAxisEnd = center + yDir * axisLength;

  // cv::rectangle(result.diagnosticImage, bounding, cv::Scalar(255, 0, 0), 2);
  drawStyledCenterOfMass(result.diagnosticImage, center);
  drawStyledAxes(result.diagnosticImage, center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfacePcaStrategy::locateByEdgePca(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity,
  bool resolveAmbiguity) const
{
  SurfaceDefectResult result;

  if (input.empty() || searchPolygon.size() < 3)
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

  const cv::Rect roi = clampSurfaceRect(cv::boundingRect(searchPolygon), gray.size());
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

  cv::Mat polygonMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  fillPolygonMask(polygonMask, searchPolygon);
  cv::bitwise_and(edgeMask, polygonMask, edgeMask);

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
  cv::findContours(edgeMask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::polylines(result.diagnosticImage, std::vector<std::vector<cv::Point>>{searchPolygon}, true, cv::Scalar(255, 0, 0), 2);

  // Determine if there is at least one significant contour that does not span the entire ROI
  bool hasSmallSignificantContour = false;
  for (const auto& c : contours)
  {
    if (c.size() >= 30)
    {
      const cv::Rect bounding = cv::boundingRect(c);
      if (bounding.width <= 0.95 * roi.width || bounding.height <= 0.95 * roi.height)
      {
        hasSmallSignificantContour = true;
        break;
      }
    }
  }

  int bestContourIndex = -1;
  size_t bestContourPoints = 0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    if (contours[i].size() < 8)
    {
      continue;
    }

    if (hasSmallSignificantContour)
    {
      const cv::Rect bounding = cv::boundingRect(contours[i]);
      if (bounding.width > 0.95 * roi.width && bounding.height > 0.95 * roi.height)
      {
        continue;
      }
    }

    if (contours[i].size() > bestContourPoints)
    {
      bestContourIndex = i;
      bestContourPoints = contours[i].size();
    }
  }

  double bestContourArea = 0.0;
  if (bestContourIndex >= 0)
  {
    bestContourArea = std::abs(cv::contourArea(contours[bestContourIndex]));
  }

  std::vector<cv::Point> edgePixels;
  if (bestContourIndex >= 0)
  {
    edgePixels = smoothSurfaceContour(contours[bestContourIndex], 5);
    drawStyledContour(result.diagnosticImage, edgePixels, cv::Scalar(94, 197, 34));
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
  double angle = std::atan2(xDirection.y, xDirection.x);

  if (resolveAmbiguity && bestContourIndex >= 0)
  {
    const std::vector<std::vector<cv::Point>>& allContours = contours;

    int bestInternalIndex = -1;
    double bestInternalArea = 0.0;
    for (int i = 0; i < static_cast<int>(allContours.size()); ++i)
    {
      const double area = std::abs(cv::contourArea(allContours[i]));
      if (area < 0.8 * bestContourArea && area > 8.0)
      {
        if (area > bestInternalArea)
        {
          bestInternalIndex = i;
          bestInternalArea = area;
        }
      }
    }

    if (bestInternalIndex >= 0)
    {
      cv::Moments m = cv::moments(allContours[bestInternalIndex]);
      cv::Point2d internalCenter;
      if (std::abs(m.m00) > 1e-6)
      {
        internalCenter = cv::Point2d(m.m10 / m.m00, m.m01 / m.m00);
      }
      else
      {
        cv::Rect r = cv::boundingRect(allContours[bestInternalIndex]);
        internalCenter = cv::Point2d(r.x + r.width / 2.0, r.y + r.height / 2.0);
      }

      cv::drawContours(result.diagnosticImage, allContours, bestInternalIndex, cv::Scalar(0, 165, 255), 2);

      const cv::Point2d toInternal = internalCenter - center;
      const double dot = toInternal.x * xDirection.x + toInternal.y * xDirection.y;
      if (dot < 0.0)
      {
        angle = angle + CV_PI;
        if (angle > CV_PI)
        {
          angle -= 2.0 * CV_PI;
        }
        else if (angle <= -CV_PI)
        {
          angle += 2.0 * CV_PI;
        }
      }
    }
  }

  const cv::Point2d xDir(std::cos(angle), std::sin(angle));
  const cv::Point2d yDir(-xDir.y, xDir.x);

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
  result.localization.method = "edge_polygon_pca";
  result.localization.center = center;
  result.localization.angleRadians = angle;
  result.localization.score = std::min(1.0, static_cast<double>(edgePixels.size()) / static_cast<double>(std::max(1, roi.area()) / 20));
  result.localization.inputPoints = static_cast<int>(edgePixels.size());
  result.localization.usedPoints = static_cast<int>(edgePixels.size());
  result.localization.xAxisStart = center - xDir * axisLength;
  result.localization.xAxisEnd = center + xDir * axisLength;
  result.localization.yAxisStart = center - yDir * axisLength;
  result.localization.yAxisEnd = center + yDir * axisLength;

  // cv::rectangle(result.diagnosticImage, bounding, cv::Scalar(255, 0, 0), 2);
  drawStyledCenterOfMass(result.diagnosticImage, center);
  drawStyledAxes(result.diagnosticImage, center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);

  result.processed = true;
  return result;
}
