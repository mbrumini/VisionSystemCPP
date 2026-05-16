#pragma once

#include "processing/SurfaceDefectProcessor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

inline cv::Rect clampSurfaceRect(const cv::Rect& rect, const cv::Size& imageSize)
{
  return rect & cv::Rect(0, 0, imageSize.width, imageSize.height);
}

inline cv::Point surfacePoint(const cv::Point2d& point)
{
  return cv::Point(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)));
}

inline void drawSurfaceCenterOfMass(cv::Mat& image, const cv::Point2d& center)
{
  const cv::Point point = surfacePoint(center);
  cv::drawMarker(image, point, cv::Scalar(0, 0, 0), cv::MARKER_CROSS, 32, 5, cv::LINE_AA);
  cv::drawMarker(image, point, cv::Scalar(0, 255, 255), cv::MARKER_CROSS, 32, 2, cv::LINE_AA);
  cv::circle(image, point, 5, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
}

inline void sortSurfaceBlobsByArea(SurfaceDefectResult& result)
{
  std::sort(result.blobs.begin(), result.blobs.end(), [](const SurfaceBlob& left, const SurfaceBlob& right) {
    return left.area > right.area;
  });
}

inline void drawSurfaceBlobs(cv::Mat& image, const std::vector<SurfaceBlob>& blobs)
{
  for (const SurfaceBlob& blob : blobs)
  {
    cv::drawContours(image, std::vector<std::vector<cv::Point>>{blob.contour}, 0, cv::Scalar(0, 255, 0), 2);
    cv::rectangle(image, blob.boundingRect, cv::Scalar(255, 0, 0), 2);
    drawSurfaceCenterOfMass(image, blob.center);
  }
}
