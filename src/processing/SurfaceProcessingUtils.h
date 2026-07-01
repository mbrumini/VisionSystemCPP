#pragma once

#include "processing/SurfaceDefectProcessor.h"

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <limits>

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

  // 1. Shadow for reticle ring (thickness reduced from 4 to 2)
  cv::circle(image, point, 14, cv::Scalar(16, 16, 16), 2, cv::LINE_AA);
  // 2. Cyan/Yellow reticle ring
  cv::circle(image, point, 14, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

  // 3. Ticks pointing inwards (shadow thickness reduced from 3 to 2)
  cv::line(image, cv::Point(point.x - 20, point.y), cv::Point(point.x - 14, point.y), cv::Scalar(16, 16, 16), 2, cv::LINE_AA);
  cv::line(image, cv::Point(point.x + 14, point.y), cv::Point(point.x + 20, point.y), cv::Scalar(16, 16, 16), 2, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y - 20), cv::Point(point.x, point.y - 14), cv::Scalar(16, 16, 16), 2, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y + 14), cv::Point(point.x, point.y + 20), cv::Scalar(16, 16, 16), 2, cv::LINE_AA);

  cv::line(image, cv::Point(point.x - 20, point.y), cv::Point(point.x - 14, point.y), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x + 14, point.y), cv::Point(point.x + 20, point.y), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y - 20), cv::Point(point.x, point.y - 14), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
  cv::line(image, cv::Point(point.x, point.y + 14), cv::Point(point.x, point.y + 20), cv::Scalar(255, 255, 0), 1, cv::LINE_AA);

  // 4. Center Dot (radius reduced from 4/2 to 3/1)
  cv::circle(image, point, 3, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
  cv::circle(image, point, 1, cv::Scalar(0, 165, 255), -1, cv::LINE_AA);
}

inline void drawStyledAxis(cv::Mat& image, const cv::Point& center, const cv::Point& start, const cv::Point& end, const cv::Scalar& color, const std::string& label)
{
  // Draw negative side (start to center) as a plain line (shadow from 5 to 3, color from 2 to 1)
  cv::line(image, start, center, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::line(image, start, center, color, 1, cv::LINE_AA);

  // Draw positive side (center to end) as an arrowed line (shadow from 5 to 3, color from 2 to 1)
  cv::arrowedLine(image, center, end, cv::Scalar(16, 16, 16), 3, cv::LINE_AA, 0, 0.15);
  cv::arrowedLine(image, center, end, color, 1, cv::LINE_AA, 0, 0.15);

  if (!label.empty())
  {
    const cv::Point textPos = end + cv::Point(10, -10);
    // Shadowed text (shadow thickness from 4 to 2, text thickness from 2 to 1, fontScale from 0.6 to 0.5)
    cv::putText(image, label, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(16, 16, 16), 2, cv::LINE_AA);
    cv::putText(image, label, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv::LINE_AA);
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

inline cv::Point2d surfaceContourCentroid(const std::vector<cv::Point>& contour)
{
  const cv::Moments moments = cv::moments(contour);
  if (std::abs(moments.m00) <= 1e-6)
  {
    const cv::Rect bounds = cv::boundingRect(contour);
    return cv::Point2d(
      bounds.x + bounds.width / 2.0,
      bounds.y + bounds.height / 2.0);
  }

  return cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
}

inline cv::Point2d surfaceMaskCentroid(
  const cv::Mat& mask,
  const cv::Point2d& origin = cv::Point2d())
{
  double sumX = 0.0;
  double sumY = 0.0;
  double count = 0.0;

  for (int row = 0; row < mask.rows; ++row)
  {
    const uchar* rowPtr = mask.ptr<uchar>(row);
    for (int col = 0; col < mask.cols; ++col)
    {
      if (rowPtr[col] == 0)
      {
        continue;
      }

      sumX += origin.x + col;
      sumY += origin.y + row;
      count += 1.0;
    }
  }

  if (count <= 0.0)
  {
    return origin;
  }

  return cv::Point2d(sumX / count, sumY / count);
}

inline cv::Point2d surfaceGeometricCenterFromContour(const std::vector<cv::Point>& contour)
{
  if (contour.size() < 3)
  {
    const cv::Rect bounds = cv::boundingRect(contour);
    return cv::Point2d(
      bounds.x + bounds.width / 2.0,
      bounds.y + bounds.height / 2.0);
  }

  std::vector<cv::Point> hull;
  cv::convexHull(contour, hull);
  if (hull.size() < 3)
  {
    const cv::Rect bounds = cv::boundingRect(contour);
    return cv::Point2d(
      bounds.x + bounds.width / 2.0,
      bounds.y + bounds.height / 2.0);
  }

  return cv::minAreaRect(hull).center;
}

inline cv::Point2d surfaceLongAxisFromContour(const std::vector<cv::Point>& contour)
{
  if (contour.size() < 3)
  {
    return cv::Point2d(1.0, 0.0);
  }

  const cv::RotatedRect fitted = cv::minAreaRect(contour);
  cv::Point2f box[4];
  fitted.points(box);
  auto edgeVector = [&](int from, int to) {
    return cv::Point2d(box[to].x - box[from].x, box[to].y - box[from].y);
  };

  const cv::Point2d edge0 = edgeVector(0, 1);
  const cv::Point2d edge1 = edgeVector(1, 2);
  const double length0 = std::hypot(edge0.x, edge0.y);
  const double length1 = std::hypot(edge1.x, edge1.y);
  cv::Point2d longEdge = length0 >= length1 ? edge0 : edge1;
  const double length = std::hypot(longEdge.x, longEdge.y);
  if (length <= 1e-6)
  {
    return cv::Point2d(1.0, 0.0);
  }

  return cv::Point2d(longEdge.x / length, longEdge.y / length);
}

inline bool isElongatedContour(const std::vector<cv::Point>& contour, double ratioThreshold = 1.08)
{
  if (contour.size() < 3)
  {
    return false;
  }

  const cv::RotatedRect fitted = cv::minAreaRect(contour);
  const double width = fitted.size.width;
  const double height = fitted.size.height;
  if (width <= 1e-3 || height <= 1e-3)
  {
    return false;
  }

  const double longer = std::max(width, height);
  const double shorter = std::min(width, height);
  return longer / shorter > ratioThreshold;
}

inline void assignLocalizationAxesLongSideOnY(
  double& angleRadians,
  cv::Point2d& xDirection,
  cv::Point2d& yDirection,
  const std::vector<cv::Point>& contour)
{
  const double xLength = std::hypot(xDirection.x, xDirection.y);
  if (xLength <= 1e-6)
  {
    xDirection = cv::Point2d(1.0, 0.0);
  }
  else
  {
    xDirection.x /= xLength;
    xDirection.y /= xLength;
  }

  yDirection = cv::Point2d(-xDirection.y, xDirection.x);
  if (!isElongatedContour(contour))
  {
    angleRadians = std::atan2(xDirection.y, xDirection.x);
    return;
  }

  const cv::Point2d longDirection = surfaceLongAxisFromContour(contour);
  yDirection = longDirection;
  xDirection = cv::Point2d(-longDirection.y, longDirection.x);
  angleRadians = std::atan2(xDirection.y, xDirection.x);
}

inline cv::Point2d orientAxisTowardMassAsymmetry(
  const cv::Point2d& geometricCenter,
  cv::Point2d axisDirection,
  const cv::Mat& mask,
  const cv::Point2d& maskOrigin,
  const cv::Rect& referenceBounds)
{
  if (axisDirection.x == 0.0 && axisDirection.y == 0.0)
  {
    return axisDirection;
  }

  const double axisLength = std::hypot(axisDirection.x, axisDirection.y);
  axisDirection.x /= axisLength;
  axisDirection.y /= axisLength;

  const cv::Point2d massCenter = surfaceMaskCentroid(mask, maskOrigin);
  const cv::Point2d offset(massCenter.x - geometricCenter.x, massCenter.y - geometricCenter.y);
  const double minOffset =
    0.03 * std::max(20.0, std::hypot(referenceBounds.width, referenceBounds.height));
  const double projection = offset.x * axisDirection.x + offset.y * axisDirection.y;
  if (std::hypot(offset.x, offset.y) >= minOffset && std::abs(projection) >= minOffset * 0.25)
  {
    if (projection < 0.0)
    {
      axisDirection = -axisDirection;
    }
  }

  return axisDirection;
}

struct HalfPlaneMassResult
{
  cv::Point2d axis;
  double positiveMass = 0.0;
  double negativeMass = 0.0;

  bool ambiguous(double ratioThreshold = 0.08) const
  {
    const double total = positiveMass + negativeMass;
    return total <= 0.0 || std::abs(positiveMass - negativeMass) / total < ratioThreshold;
  }
};

inline HalfPlaneMassResult measureHalfPlaneMass(
  const cv::Point2d& center,
  cv::Point2d axisDirection,
  const cv::Mat& mask,
  const cv::Point2d& maskOrigin)
{
  HalfPlaneMassResult result;
  result.axis = axisDirection;

  if (axisDirection.x == 0.0 && axisDirection.y == 0.0)
  {
    return result;
  }

  const double axisLength = std::hypot(axisDirection.x, axisDirection.y);
  axisDirection.x /= axisLength;
  axisDirection.y /= axisLength;
  result.axis = axisDirection;

  for (int row = 0; row < mask.rows; ++row)
  {
    const uchar* rowPtr = mask.ptr<uchar>(row);
    for (int col = 0; col < mask.cols; ++col)
    {
      if (rowPtr[col] == 0)
      {
        continue;
      }

      const double projection =
        (maskOrigin.x + col - center.x) * axisDirection.x +
        (maskOrigin.y + row - center.y) * axisDirection.y;
      if (projection > 0.0)
      {
        result.positiveMass += 1.0;
      }
      else if (projection < 0.0)
      {
        result.negativeMass += 1.0;
      }
    }
  }

  return result;
}

inline HalfPlaneMassResult orientAxisByHalfPlaneMass(
  const cv::Point2d& center,
  cv::Point2d axisDirection,
  const cv::Mat& mask,
  const cv::Point2d& maskOrigin)
{
  HalfPlaneMassResult result = measureHalfPlaneMass(center, axisDirection, mask, maskOrigin);
  axisDirection = result.axis;

  if (result.negativeMass > result.positiveMass)
  {
    axisDirection = -axisDirection;
  }

  result.axis = axisDirection;
  return result;
}

inline cv::Point2d orientAxisToReferenceHalfPlane(
  const cv::Point2d& center,
  cv::Point2d axisDirection,
  const cv::Mat& mask,
  const cv::Point2d& maskOrigin,
  bool referencePositiveHalfPlane)
{
  if (axisDirection.x == 0.0 && axisDirection.y == 0.0)
  {
    return axisDirection;
  }

  const HalfPlaneMassResult halfPlane = measureHalfPlaneMass(
    center,
    axisDirection,
    mask,
    maskOrigin);
  const bool positiveHalfPlane = halfPlane.positiveMass > halfPlane.negativeMass;
  if (positiveHalfPlane != referencePositiveHalfPlane)
  {
    axisDirection = -axisDirection;
  }

  return axisDirection;
}

inline cv::Point2d resolveSurfacePcaInternalAmbiguity(
  const cv::Point2d& center,
  cv::Point2d xDirection,
  const std::vector<std::vector<cv::Point>>& contours,
  const cv::Rect& referenceBounds,
  double mainContourArea,
  int* selectedInternalContourIndex = nullptr)
{
  if (selectedInternalContourIndex != nullptr)
  {
    *selectedInternalContourIndex = -1;
  }

  const double minOffset =
    0.05 * std::max(20.0, std::hypot(referenceBounds.width, referenceBounds.height));

  auto orientToward = [&](const cv::Point2d& target) {
    const cv::Point2d offset = target - center;
    const double projection = offset.x * xDirection.x + offset.y * xDirection.y;
    if (projection < 0.0)
    {
      xDirection = -xDirection;
    }
  };

  int bestInternalIndex = -1;
  double bestInternalDistance = 0.0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = std::abs(cv::contourArea(contours[i]));
    if (area >= 0.8 * mainContourArea || area <= 8.0)
    {
      continue;
    }

    const cv::Point2d internalCenter = surfaceContourCentroid(contours[i]);
    const double distance = std::hypot(internalCenter.x - center.x, internalCenter.y - center.y);
    if (distance < minOffset || distance <= bestInternalDistance)
    {
      continue;
    }

    bestInternalIndex = i;
    bestInternalDistance = distance;
  }

  if (bestInternalIndex >= 0)
  {
    orientToward(surfaceContourCentroid(contours[bestInternalIndex]));
    if (selectedInternalContourIndex != nullptr)
    {
      *selectedInternalContourIndex = bestInternalIndex;
    }
  }

  return xDirection;
}

inline cv::Point2d resolveSurfacePcaAxisDirection(
  const cv::Point2d& center,
  cv::Point2d xDirection,
  const cv::Mat& weightMask,
  const cv::Point2d& maskOrigin,
  const cv::Rect& referenceBounds,
  bool resolveAmbiguity,
  const std::vector<std::vector<cv::Point>>* contours,
  double mainContourArea,
  int* selectedInternalContourIndex = nullptr)
{
  if (selectedInternalContourIndex != nullptr)
  {
    *selectedInternalContourIndex = -1;
  }

  const double minOffset =
    0.05 * std::max(20.0, std::hypot(referenceBounds.width, referenceBounds.height));

  auto orientToward = [&](const cv::Point2d& target) {
    const cv::Point2d offset = target - center;
    const double projection = offset.x * xDirection.x + offset.y * xDirection.y;
    if (projection < 0.0)
    {
      xDirection = -xDirection;
    }
  };

  if (resolveAmbiguity && contours != nullptr && mainContourArea > 0.0)
  {
    int bestInternalIndex = -1;
    double bestInternalDistance = 0.0;
    for (int i = 0; i < static_cast<int>(contours->size()); ++i)
    {
      const double area = std::abs(cv::contourArea((*contours)[i]));
      if (area >= 0.8 * mainContourArea || area <= 8.0)
      {
        continue;
      }

      const cv::Point2d internalCenter = surfaceContourCentroid((*contours)[i]);
      const double distance = std::hypot(internalCenter.x - center.x, internalCenter.y - center.y);
      if (distance < minOffset || distance <= bestInternalDistance)
      {
        continue;
      }

      bestInternalIndex = i;
      bestInternalDistance = distance;
    }

    if (bestInternalIndex >= 0)
    {
      orientToward(surfaceContourCentroid((*contours)[bestInternalIndex]));
      if (selectedInternalContourIndex != nullptr)
      {
        *selectedInternalContourIndex = bestInternalIndex;
      }
      return xDirection;
    }
  }

  const cv::Point2d maskCentroid = surfaceMaskCentroid(weightMask, maskOrigin);
  const cv::Point2d centroidOffset(maskCentroid.x - center.x, maskCentroid.y - center.y);
  const double centroidProjection =
    centroidOffset.x * xDirection.x + centroidOffset.y * xDirection.y;
  if (std::abs(centroidProjection) >= minOffset * 0.5)
  {
    orientToward(maskCentroid);
    return xDirection;
  }

  double positiveWeight = 0.0;
  double negativeWeight = 0.0;
  for (int row = 0; row < weightMask.rows; ++row)
  {
    const uchar* rowPtr = weightMask.ptr<uchar>(row);
    for (int col = 0; col < weightMask.cols; ++col)
    {
      if (rowPtr[col] == 0)
      {
        continue;
      }

      const double projection =
        (maskOrigin.x + col - center.x) * xDirection.x +
        (maskOrigin.y + row - center.y) * xDirection.y;
      const double weight = std::abs(projection) + 1.0;
      if (projection > 0.0)
      {
        positiveWeight += weight;
      }
      else if (projection < 0.0)
      {
        negativeWeight += weight;
      }
    }
  }

  if (negativeWeight > positiveWeight)
  {
    xDirection = -xDirection;
  }

  return xDirection;
}

inline void drawSurfaceBlobs(cv::Mat& image, const std::vector<SurfaceBlob>& blobs, bool drawContours = true)
{
  for (const SurfaceBlob& blob : blobs)
  {
    if (drawContours)
    {
      drawStyledContour(image, blob.contour, cv::Scalar(94, 197, 34));
    }
    drawStyledCenterOfMass(image, blob.center);
  }
}
