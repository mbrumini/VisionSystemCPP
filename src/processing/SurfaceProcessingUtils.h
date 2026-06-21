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

inline std::vector<cv::Point> smoothSurfaceContour(const std::vector<cv::Point>& contour, int windowSize = 5)
{
  if (contour.size() < static_cast<size_t>(windowSize) || windowSize <= 1)
  {
    return contour;
  }

  std::vector<cv::Point> smoothed;
  smoothed.reserve(contour.size());

  const int n = static_cast<int>(contour.size());
  const int half = windowSize / 2;

  for (int i = 0; i < n; ++i)
  {
    double sumX = 0;
    double sumY = 0;
    int count = 0;
    for (int w = -half; w <= half; ++w)
    {
      int idx = i + w;
      if (idx >= 0 && idx < n)
      {
        sumX += contour[idx].x;
        sumY += contour[idx].y;
        count++;
      }
    }
    smoothed.emplace_back(
      static_cast<int>(std::round(sumX / count)),
      static_cast<int>(std::round(sumY / count))
    );
  }

  return smoothed;
}

inline void drawStyledContour(cv::Mat& image, const std::vector<cv::Point>& contour, const cv::Scalar& color = cv::Scalar(94, 197, 34))
{
  if (contour.empty())
  {
    return;
  }
  // Thicker dark shadow at the bottom
  cv::drawContours(image, std::vector<std::vector<cv::Point>>{contour}, -1, cv::Scalar(16, 20, 16), 5, cv::LINE_AA);
  // Semi-transparent colored glow outline
  cv::drawContours(image, std::vector<std::vector<cv::Point>>{contour}, -1, color * 0.6, 3, cv::LINE_AA);
  // Main foreground thin line
  cv::drawContours(image, std::vector<std::vector<cv::Point>>{contour}, -1, color, 1, cv::LINE_AA);
}

inline void drawStyledCenterOfMass(cv::Mat& image, const cv::Point2d& center)
{
  const cv::Point point = surfacePoint(center);

  // 1. Shadow for reticle ring
  cv::circle(image, point, 14, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
  // 2. Cyan/Yellow reticle ring
  cv::circle(image, point, 14, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

  // 3. Ticks pointing inwards (shadow first, then cyan line)
  cv::line(image, cv::Point(point.x - 20, point.y), cv::Point(point.x - 14, point.y), cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::line(image, cv::Point(point.x + 14, point.y), cv::Point(point.x + 20, point.y), cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y - 20), cv::Point(point.x, point.y - 14), cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y + 14), cv::Point(point.x, point.y + 20), cv::Scalar(16, 16, 16), 3, cv::LINE_AA);

  cv::line(image, cv::Point(point.x - 20, point.y), cv::Point(point.x - 14, point.y), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x + 14, point.y), cv::Point(point.x + 20, point.y), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y - 20), cv::Point(point.x, point.y - 14), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y + 14), cv::Point(point.x, point.y + 20), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

  // 4. Center Dot (orange core with black shadow border)
  cv::circle(image, point, 4, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
  cv::circle(image, point, 2, cv::Scalar(0, 165, 255), -1, cv::LINE_AA);
}

inline void drawStyledAxis(cv::Mat& image, const cv::Point& center, const cv::Point& start, const cv::Point& end, const cv::Scalar& color, const std::string& label)
{
  // Draw negative side (start to center) as a plain line
  cv::line(image, start, center, cv::Scalar(16, 16, 16), 5, cv::LINE_AA);
  cv::line(image, start, center, color, 2, cv::LINE_AA);

  // Draw positive side (center to end) as an arrowed line
  cv::arrowedLine(image, center, end, cv::Scalar(16, 16, 16), 5, cv::LINE_AA, 0, 0.15);
  cv::arrowedLine(image, center, end, color, 2, cv::LINE_AA, 0, 0.15);

  if (!label.empty())
  {
    const cv::Point textPos = end + cv::Point(10, -10);
    // Shadowed text
    cv::putText(image, label, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
    cv::putText(image, label, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2, cv::LINE_AA);
  }
}

inline void drawStyledAxes(cv::Mat& image, const cv::Point2d& centerPoint, const cv::Point2d& xStart, const cv::Point2d& xEnd, const cv::Point2d& yStart, const cv::Point2d& yEnd, const cv::Scalar& xColor = cv::Scalar(0, 0, 255), const cv::Scalar& yColor = cv::Scalar(255, 255, 0))
{
  const cv::Point center = surfacePoint(centerPoint);
  drawStyledAxis(image, center, surfacePoint(xStart), surfacePoint(xEnd), xColor, "X");
  drawStyledAxis(image, center, surfacePoint(yStart), surfacePoint(yEnd), yColor, "Y");
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
    drawStyledContour(image, blob.contour, cv::Scalar(94, 197, 34));
    drawStyledCenterOfMass(image, blob.center);
  }
}
