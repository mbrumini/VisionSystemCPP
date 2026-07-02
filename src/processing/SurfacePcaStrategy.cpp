#include "SurfacePcaStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
double contourProfileArea(const std::vector<cv::Point>& contour)
{
  std::vector<cv::Point> hull;
  cv::convexHull(contour, hull);
  return hull.size() >= 3 ? std::abs(cv::contourArea(hull)) : std::abs(cv::contourArea(contour));
}

std::vector<int> significantContourIndexes(
  const std::vector<std::vector<cv::Point>>& contours,
  const cv::Rect& searchBounds)
{
  std::vector<int> indexes;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    if (contours[i].size() < 8)
    {
      continue;
    }

    const cv::Rect bounds = cv::boundingRect(contours[i]);
    if (bounds.width > 0.98 * searchBounds.width && bounds.height > 0.98 * searchBounds.height)
    {
      continue;
    }

    indexes.push_back(i);
  }
  return indexes;
}

cv::Rect contourUnionBounds(
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<int>& indexes)
{
  cv::Rect bounds;
  bool initialized = false;
  for (const int index : indexes)
  {
    const cv::Rect contourBounds = cv::boundingRect(contours[index]);
    if (!initialized)
    {
      bounds = contourBounds;
      initialized = true;
    }
    else
    {
      bounds |= contourBounds;
    }
  }
  return initialized ? bounds : cv::Rect();
}

std::vector<int> externalContourIndexes(
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<int>& indexes,
  const cv::Rect& unionBounds)
{
  std::vector<int> external;
  if (unionBounds.empty())
  {
    return external;
  }

  const int margin = std::max(4, static_cast<int>(std::round(0.04 * std::max(unionBounds.width, unionBounds.height))));
  for (const int index : indexes)
  {
    const cv::Rect bounds = cv::boundingRect(contours[index]);
    const bool touchesOuterEnvelope =
      bounds.x <= unionBounds.x + margin ||
      bounds.y <= unionBounds.y + margin ||
      bounds.br().x >= unionBounds.br().x - margin ||
      bounds.br().y >= unionBounds.br().y - margin;
    if (touchesOuterEnvelope)
    {
      external.push_back(index);
    }
  }
  return external;
}

struct EdgePcaContourGroups
{
  std::vector<int> externalIndexes;
  std::vector<int> internalIndexes;
  cv::Rect profileBounds;
  double mainContourArea = 0.0;
};

EdgePcaContourGroups classifyEdgePcaContourGroups(
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<cv::Vec4i>& hierarchy,
  const cv::Rect& searchBounds)
{
  EdgePcaContourGroups groups;
  const std::vector<int> significantIndexes = significantContourIndexes(contours, searchBounds);
  groups.profileBounds = contourUnionBounds(contours, significantIndexes);

  int mainExternalIndex = -1;
  double bestExternalArea = 0.0;
  for (const int index : significantIndexes)
  {
    if (index >= static_cast<int>(hierarchy.size()) || hierarchy[index][3] != -1)
    {
      continue;
    }

    const double area = contourProfileArea(contours[index]);
    if (area > bestExternalArea)
    {
      bestExternalArea = area;
      mainExternalIndex = index;
    }
  }

  if (mainExternalIndex >= 0)
  {
    groups.externalIndexes.push_back(mainExternalIndex);
    groups.mainContourArea = bestExternalArea;
    groups.profileBounds = cv::boundingRect(contours[mainExternalIndex]);
    const cv::Rect mainBounds = groups.profileBounds;

    for (const int index : significantIndexes)
    {
      if (index == mainExternalIndex)
      {
        continue;
      }

      if (index < static_cast<int>(hierarchy.size()) && hierarchy[index][3] == mainExternalIndex)
      {
        groups.internalIndexes.push_back(index);
        continue;
      }

      const cv::Rect bounds = cv::boundingRect(contours[index]);
      const cv::Point center(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
      if (mainBounds.contains(center))
      {
        groups.internalIndexes.push_back(index);
      }
    }

    return groups;
  }

  groups.externalIndexes = externalContourIndexes(contours, significantIndexes, groups.profileBounds);
  if (groups.externalIndexes.empty())
  {
    groups.externalIndexes = significantIndexes;
  }

  for (const int index : significantIndexes)
  {
    if (std::find(groups.externalIndexes.begin(), groups.externalIndexes.end(), index) == groups.externalIndexes.end())
    {
      groups.internalIndexes.push_back(index);
    }
  }

  if (!groups.externalIndexes.empty())
  {
    groups.mainContourArea = contourProfileArea(contours[groups.externalIndexes[0]]);
  }

  return groups;
}

void appendContourPoints(
  std::vector<cv::Point>& points,
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<int>& indexes)
{
  for (const int index : indexes)
  {
    points.insert(points.end(), contours[index].begin(), contours[index].end());
  }
}

bool contourSpansSearchArea(const std::vector<cv::Point>& contour, const cv::Rect& searchBounds)
{
  const cv::Rect bounding = cv::boundingRect(contour);
  return bounding.width > 0.95 * searchBounds.width && bounding.height > 0.95 * searchBounds.height;
}

std::vector<int> internalContourIndexes(
  const std::vector<std::vector<cv::Point>>& contours,
  int mainContourIndex,
  double mainContourArea,
  const cv::Rect& mainBounds)
{
  std::vector<int> indexes;
  const double minArea = std::max(8.0, mainContourArea * 0.002);
  const cv::Point2d mainCenter(
    mainBounds.x + mainBounds.width / 2.0,
    mainBounds.y + mainBounds.height / 2.0);

  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    if (i == mainContourIndex || contours[i].size() < 8)
    {
      continue;
    }

    const double area = std::abs(cv::contourArea(contours[i]));
    if (area <= minArea || area >= 0.8 * mainContourArea)
    {
      continue;
    }

    const cv::Rect bounds = cv::boundingRect(contours[i]);
    const cv::Point2d center(bounds.x + bounds.width / 2.0, bounds.y + bounds.height / 2.0);
    if (!mainBounds.contains(cv::Point(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)))))
    {
      continue;
    }

    if (std::hypot(center.x - mainCenter.x, center.y - mainCenter.y) < 2.0)
    {
      continue;
    }

    indexes.push_back(i);
  }

  return indexes;
}

void drawEdgePcaContours(
  cv::Mat& image,
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<int>& externalIndexes,
  const std::vector<int>& internalIndexes,
  int selectedInternalContourIndex,
  bool emphasize = false)
{
  const int thickness = emphasize ? 3 : 1;
  for (const int index : externalIndexes)
  {
    const std::vector<cv::Point> smoothed = smoothSurfaceContour(contours[index], 3);
    cv::drawContours(image, std::vector<std::vector<cv::Point>>{smoothed}, -1, cv::Scalar(16, 20, 16), thickness + 3, cv::LINE_AA);
    cv::drawContours(image, std::vector<std::vector<cv::Point>>{smoothed}, -1, cv::Scalar(94, 197, 34), thickness + 1, cv::LINE_AA);
    drawStyledContour(image, smoothed, cv::Scalar(94, 197, 34));
  }

  for (const int index : internalIndexes)
  {
    const cv::Scalar color = index == selectedInternalContourIndex ? cv::Scalar(0, 220, 255) : cv::Scalar(0, 165, 255);
    const std::vector<cv::Point> smoothed = smoothSurfaceContour(contours[index], 3);
    cv::drawContours(image, std::vector<std::vector<cv::Point>>{smoothed}, -1, cv::Scalar(16, 20, 16), thickness + 3, cv::LINE_AA);
    cv::drawContours(image, std::vector<std::vector<cv::Point>>{smoothed}, -1, color, thickness + 1, cv::LINE_AA);
    drawStyledContour(image, smoothed, color);
  }
}

void fillPolygonMask(cv::Mat& mask, const std::vector<cv::Point>& polygon)
{
  if (polygon.size() < 3)
  {
    return;
  }

  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{polygon}, cv::Scalar(255));
}

void applyExclusionsToMask(cv::Mat& mask, const std::vector<cv::Rect>& exclusionRects)
{
  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect maskExclusion = clampSurfaceRect(exclusionRect, mask.size());
    if (!maskExclusion.empty())
    {
      mask(maskExclusion).setTo(0);
    }
  }
}

int estimateSilhouetteDarkThreshold(const cv::Mat& gray, const cv::Mat& searchMask)
{
  std::vector<uchar> samples;
  samples.reserve(65536);
  for (int y = 0; y < gray.rows; ++y)
  {
    const uchar* maskRow = searchMask.ptr<uchar>(y);
    const uchar* grayRow = gray.ptr<uchar>(y);
    for (int x = 0; x < gray.cols; ++x)
    {
      if (maskRow[x] != 0)
      {
        samples.push_back(grayRow[x]);
      }
    }
  }

  if (samples.size() < 256)
  {
    return 120;
  }

  cv::Mat sampleMat(1, static_cast<int>(samples.size()), CV_8UC1);
  std::memcpy(sampleMat.data, samples.data(), samples.size());
  const double otsu = cv::threshold(sampleMat, sampleMat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  return std::clamp(static_cast<int>(std::round(otsu * 0.92)), 45, 190);
}

struct EdgePcaPreparedData
{
  cv::Mat edgeMask;
  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  EdgePcaContourGroups groups;
  bool usedSilhouette = false;
};

bool tryPrepareSilhouetteEdgePcaData(
  const cv::Mat& gray,
  const cv::Mat& searchMask,
  const cv::Rect& roi,
  const std::vector<cv::Rect>& exclusionRects,
  EdgePcaPreparedData& data)
{
  const int estimated = estimateSilhouetteDarkThreshold(gray, searchMask);
  const std::vector<int> candidates = {
    estimated,
    std::max(60, estimated - 20),
    std::min(180, estimated + 20),
    90,
    110};

  int bestExternalIndex = -1;
  double bestExternalArea = 0.0;
  std::vector<std::vector<cv::Point>> bestContours;
  std::vector<cv::Vec4i> bestHierarchy;
  EdgePcaContourGroups bestGroups;

  for (const int darkMax : candidates)
  {
    cv::Mat darkMask;
    cv::inRange(gray, cv::Scalar(0), cv::Scalar(darkMax), darkMask);
    cv::bitwise_and(darkMask, searchMask, darkMask);
    applyExclusionsToMask(darkMask, exclusionRects);

    cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(darkMask, darkMask, cv::MORPH_CLOSE, closeKernel);
    cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(darkMask, darkMask, cv::MORPH_OPEN, openKernel);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(darkMask, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);

    const EdgePcaContourGroups groups = classifyEdgePcaContourGroups(contours, hierarchy, roi);
    if (groups.externalIndexes.empty() || groups.mainContourArea < roi.area() * 0.02)
    {
      continue;
    }

    if (groups.mainContourArea > bestExternalArea)
    {
      bestExternalArea = groups.mainContourArea;
      bestExternalIndex = groups.externalIndexes.front();
      bestContours = std::move(contours);
      bestHierarchy = std::move(hierarchy);
      bestGroups = groups;
    }
  }

  if (bestExternalIndex < 0)
  {
    return false;
  }

  data.edgeMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  for (const int index : bestGroups.externalIndexes)
  {
    cv::drawContours(data.edgeMask, bestContours, index, cv::Scalar(255), 1, cv::LINE_8);
  }
  for (const int index : bestGroups.internalIndexes)
  {
    cv::drawContours(data.edgeMask, bestContours, index, cv::Scalar(255), 1, cv::LINE_8);
  }

  data.contours = std::move(bestContours);
  data.hierarchy = std::move(bestHierarchy);
  data.groups = std::move(bestGroups);
  data.usedSilhouette = true;
  return true;
}

void prepareCannyEdgePcaData(
  const cv::Mat& blurred,
  const cv::Mat& searchMask,
  const cv::Rect& roi,
  int edgeSensitivity,
  const std::vector<cv::Rect>& exclusionRects,
  EdgePcaPreparedData& data)
{
  const int high = std::clamp(edgeSensitivity, 1, 255);
  const int low = std::max(1, high / 2);
  cv::Canny(blurred, data.edgeMask, low, high);
  cv::bitwise_and(data.edgeMask, searchMask, data.edgeMask);
  applyExclusionsToMask(data.edgeMask, exclusionRects);

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::morphologyEx(data.edgeMask, data.edgeMask, cv::MORPH_CLOSE, kernel);

  cv::findContours(data.edgeMask, data.contours, data.hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
  data.groups = classifyEdgePcaContourGroups(data.contours, data.hierarchy, roi);
  data.usedSilhouette = false;
}

cv::Point2d contourCentroidFromMoments(const std::vector<cv::Point>& contour)
{
  const cv::Moments moments = cv::moments(contour, true);
  if (std::abs(moments.m00) <= std::numeric_limits<double>::epsilon())
  {
    return surfaceContourCentroid(contour);
  }

  return cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
}

cv::Point2d orientAxisTowardInternalFeatures(
  const cv::Point2d& center,
  cv::Point2d axis,
  const std::vector<std::vector<cv::Point>>& contours,
  const std::vector<int>& internalIndexes,
  int* selectedInternalContourIndex = nullptr)
{
  if (selectedInternalContourIndex != nullptr)
  {
    *selectedInternalContourIndex = -1;
  }

  int bestIndex = -1;
  double bestDistance = 0.0;
  for (const int index : internalIndexes)
  {
    if (index < 0 || index >= static_cast<int>(contours.size()) || contours[index].size() < 8)
    {
      continue;
    }

    const cv::Point2d internalCenter = contourCentroidFromMoments(contours[index]);
    const double distance = std::hypot(internalCenter.x - center.x, internalCenter.y - center.y);
    if (distance <= bestDistance)
    {
      continue;
    }

    bestDistance = distance;
    bestIndex = index;
  }

  if (bestIndex < 0 || bestDistance < 8.0)
  {
    return axis;
  }

  const cv::Point2d internalCenter = contourCentroidFromMoments(contours[bestIndex]);
  const cv::Point2d offset = internalCenter - center;
  const double projection = offset.x * axis.x + offset.y * axis.y;
  if (projection < 0.0)
  {
    axis = -axis;
  }

  if (selectedInternalContourIndex != nullptr)
  {
    *selectedInternalContourIndex = bestIndex;
  }

  return axis;
}

SurfaceDefectResult finalizeEdgePcaLocalization(
  const cv::Mat& input,
  const cv::Mat& gray,
  const cv::Rect& roi,
  const EdgePcaPreparedData& data,
  bool resolveAmbiguity,
  bool createDiagnosticImage,
  bool drawContours)
{
  SurfaceDefectResult result;

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

  const std::vector<int>& externalIndexes = data.groups.externalIndexes;
  const std::vector<int>& internalIndexes = data.groups.internalIndexes;
  const cv::Rect profileBounds = data.groups.profileBounds;

  std::vector<cv::Point> edgePixels;
  appendContourPoints(edgePixels, data.contours, externalIndexes);
  edgePixels = smoothSurfaceContour(edgePixels, 3);

  if (edgePixels.size() < 8)
  {
    result.processed = true;
    return result;
  }

  cv::Point2d center = surfaceGeometricCenterFromContour(edgePixels);
  if (data.usedSilhouette && !externalIndexes.empty())
  {
    center = contourCentroidFromMoments(data.contours[externalIndexes.front()]);
  }

  cv::Point2d xDirection = surfaceLongAxisFromContour(edgePixels);
  if (data.usedSilhouette && !internalIndexes.empty())
  {
    xDirection = orientAxisTowardInternalFeatures(center, xDirection, data.contours, internalIndexes);
  }

  const HalfPlaneMassResult halfPlane = orientAxisByHalfPlaneMass(
    center,
    xDirection,
    data.edgeMask(roi),
    cv::Point2d(roi.x, roi.y));
  xDirection = halfPlane.axis;

  if (resolveAmbiguity)
  {
    cv::Mat fillMask = cv::Mat::zeros(gray.size(), CV_8UC1);
    if (data.usedSilhouette && !externalIndexes.empty())
    {
      cv::drawContours(fillMask, data.contours, externalIndexes.front(), cv::Scalar(255), cv::FILLED);
    }
    else
    {
      cv::fillPoly(fillMask, std::vector<std::vector<cv::Point>>{edgePixels}, cv::Scalar(255));
    }

    xDirection = orientAxisTowardMassAsymmetry(
      center,
      xDirection,
      fillMask(roi),
      cv::Point2d(roi.x, roi.y),
      profileBounds.empty() ? roi : profileBounds);
  }

  int selectedInternalContourIndex = -1;
  if (resolveAmbiguity)
  {
    if (!internalIndexes.empty())
    {
      xDirection = orientAxisTowardInternalFeatures(
        center,
        xDirection,
        data.contours,
        internalIndexes,
        &selectedInternalContourIndex);
    }
    else
    {
      xDirection = resolveSurfacePcaInternalAmbiguity(
        center,
        xDirection,
        data.contours,
        profileBounds.empty() ? roi : profileBounds,
        std::max(1.0, data.groups.mainContourArea > 0.0 ? data.groups.mainContourArea : static_cast<double>(profileBounds.area())),
        &selectedInternalContourIndex);
    }
  }

  const double angle = std::atan2(xDirection.y, xDirection.x);

  cv::Point2d xDir(std::cos(angle), std::sin(angle));
  cv::Point2d yDir(-xDir.y, xDir.x);
  double orientedAngle = angle;
  assignLocalizationAxesLongSideOnY(orientedAngle, xDir, yDir, edgePixels);

  if (createDiagnosticImage && drawContours)
  {
    drawEdgePcaContours(
      result.diagnosticImage,
      data.contours,
      externalIndexes,
      internalIndexes,
      selectedInternalContourIndex,
      data.usedSilhouette);
  }

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
  result.localization.method = data.usedSilhouette ? "edge_silhouette_pca" : "edge_pca";
  result.localization.center = center;
  result.localization.angleRadians = orientedAngle;
  result.localization.score = std::min(1.0, static_cast<double>(edgePixels.size()) / static_cast<double>(std::max(1, roi.area()) / 20));
  result.localization.inputPoints = static_cast<int>(edgePixels.size());
  result.localization.usedPoints = static_cast<int>(edgePixels.size());
  result.localization.xAxisStart = center - xDir * axisLength;
  result.localization.xAxisEnd = center + xDir * axisLength;
  result.localization.yAxisStart = center - yDir * axisLength;
  result.localization.yAxisEnd = center + yDir * axisLength;

  if (createDiagnosticImage)
  {
    drawStyledCenterOfMass(result.diagnosticImage, center);
    drawStyledAxes(
      result.diagnosticImage,
      center,
      result.localization.xAxisStart,
      result.localization.xAxisEnd,
      result.localization.yAxisStart,
      result.localization.yAxisEnd);
  }

  result.processed = true;
  return result;
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

  cv::Mat searchMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  searchMask(roi).setTo(255);

  EdgePcaPreparedData data;
  if (!tryPrepareSilhouetteEdgePcaData(gray, searchMask, roi, exclusionRects, data))
  {
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);
    prepareCannyEdgePcaData(blurred, searchMask, roi, edgeSensitivity, exclusionRects, data);
  }

  return finalizeEdgePcaLocalization(
    input,
    gray,
    roi,
    data,
    resolveAmbiguity,
    createDiagnosticImage,
    drawContours);
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

  cv::Mat searchMask = cv::Mat::zeros(gray.size(), CV_8UC1);
  fillPolygonMask(searchMask, searchPolygon);

  EdgePcaPreparedData data;
  if (!tryPrepareSilhouetteEdgePcaData(gray, searchMask, roi, exclusionRects, data))
  {
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);
    prepareCannyEdgePcaData(blurred, searchMask, roi, edgeSensitivity, exclusionRects, data);
  }

  return finalizeEdgePcaLocalization(
    input,
    gray,
    roi,
    data,
    resolveAmbiguity,
    createDiagnosticImage,
    drawContours);
}
