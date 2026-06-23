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
  bool resolveAmbiguity,
  bool createDiagnosticImage,
  bool drawContours) const
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

  if (createDiagnosticImage)
  {
    if (input.channels() == 1)
    {
      cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
    }
    else
    {
      input.copyTo(result.diagnosticImage);
    }
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
  double bestProfileArea = 0.0;
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

    std::vector<cv::Point> hull;
    cv::convexHull(contours[i], hull);
    const double profileArea = hull.size() >= 3 ? cv::contourArea(hull) : cv::contourArea(contours[i]);
    if (profileArea <= bestProfileArea)
    {
      continue;
    }

    bestContourIndex = i;
    bestProfileArea = profileArea;
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
    if (createDiagnosticImage && drawContours)
    {
      drawStyledContour(result.diagnosticImage, edgePixels, cv::Scalar(94, 197, 34));
    }
  }

  if (edgePixels.size() < 8)
  {
    result.processed = true;
    return result;
  }

  const cv::Point2d center = surfaceGeometricCenterFromContour(edgePixels);
  cv::Point2d xDirection = surfaceLongAxisFromContour(edgePixels);

  const HalfPlaneMassResult halfPlane = orientAxisByHalfPlaneMass(
    center,
    xDirection,
    edgeMask(roi),
    cv::Point2d(roi.x, roi.y));
  xDirection = halfPlane.axis;

  if (resolveAmbiguity)
  {
    cv::Mat fillMask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::fillPoly(fillMask, std::vector<std::vector<cv::Point>>{edgePixels}, cv::Scalar(255));
    xDirection = orientAxisTowardMassAsymmetry(
      center,
      xDirection,
      fillMask(roi),
      cv::Point2d(roi.x, roi.y),
      bestContourIndex >= 0 ? cv::boundingRect(contours[bestContourIndex]) : roi);
  }

  int selectedInternalContourIndex = -1;
  if (resolveAmbiguity && halfPlane.ambiguous())
  {
    xDirection = resolveSurfacePcaInternalAmbiguity(
      center,
      xDirection,
      contours,
      bestContourIndex >= 0 ? cv::boundingRect(contours[bestContourIndex]) : roi,
      bestContourArea,
      &selectedInternalContourIndex);
  }
  const double angle = std::atan2(xDirection.y, xDirection.x);

  if (createDiagnosticImage && drawContours && selectedInternalContourIndex >= 0)
  {
    cv::drawContours(
      result.diagnosticImage,
      contours,
      selectedInternalContourIndex,
      cv::Scalar(0, 165, 255),
      2);
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
  if (createDiagnosticImage)
  {
    drawStyledCenterOfMass(result.diagnosticImage, center);
    drawStyledAxes(result.diagnosticImage, center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);
  }

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfacePcaStrategy::locateByEdgePca(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity,
  bool resolveAmbiguity,
  bool createDiagnosticImage,
  bool drawContours) const
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

  if (createDiagnosticImage)
  {
    if (input.channels() == 1)
    {
      cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
    }
    else
    {
      input.copyTo(result.diagnosticImage);
    }
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
  double bestProfileArea = 0.0;
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

    std::vector<cv::Point> hull;
    cv::convexHull(contours[i], hull);
    const double profileArea = hull.size() >= 3 ? cv::contourArea(hull) : cv::contourArea(contours[i]);
    if (profileArea <= bestProfileArea)
    {
      continue;
    }

    bestContourIndex = i;
    bestProfileArea = profileArea;
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
    if (createDiagnosticImage && drawContours)
    {
      drawStyledContour(result.diagnosticImage, edgePixels, cv::Scalar(94, 197, 34));
    }
  }

  if (edgePixels.size() < 8)
  {
    result.processed = true;
    return result;
  }

  const cv::Point2d center = surfaceGeometricCenterFromContour(edgePixels);
  cv::Point2d xDirection = surfaceLongAxisFromContour(edgePixels);

  const HalfPlaneMassResult halfPlane = orientAxisByHalfPlaneMass(
    center,
    xDirection,
    edgeMask(roi),
    cv::Point2d(roi.x, roi.y));
  xDirection = halfPlane.axis;

  if (resolveAmbiguity)
  {
    cv::Mat fillMask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::fillPoly(fillMask, std::vector<std::vector<cv::Point>>{edgePixels}, cv::Scalar(255));
    xDirection = orientAxisTowardMassAsymmetry(
      center,
      xDirection,
      fillMask(roi),
      cv::Point2d(roi.x, roi.y),
      bestContourIndex >= 0 ? cv::boundingRect(contours[bestContourIndex]) : roi);
  }

  int selectedInternalContourIndex = -1;
  if (resolveAmbiguity && halfPlane.ambiguous())
  {
    xDirection = resolveSurfacePcaInternalAmbiguity(
      center,
      xDirection,
      contours,
      bestContourIndex >= 0 ? cv::boundingRect(contours[bestContourIndex]) : roi,
      bestContourArea,
      &selectedInternalContourIndex);
  }
  const double angle = std::atan2(xDirection.y, xDirection.x);

  if (createDiagnosticImage && drawContours && selectedInternalContourIndex >= 0)
  {
    cv::drawContours(
      result.diagnosticImage,
      contours,
      selectedInternalContourIndex,
      cv::Scalar(0, 165, 255),
      2);
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
  if (createDiagnosticImage)
  {
    drawStyledCenterOfMass(result.diagnosticImage, center);
    drawStyledAxes(result.diagnosticImage, center, result.localization.xAxisStart, result.localization.xAxisEnd, result.localization.yAxisStart, result.localization.yAxisEnd);
  }

  result.processed = true;
  return result;
}
