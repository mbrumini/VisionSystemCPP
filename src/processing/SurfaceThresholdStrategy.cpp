#include "SurfaceThresholdStrategy.h"

#include "processing/SurfaceProcessingUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
std::vector<cv::Point> localPolygon(const std::vector<cv::Point>& polygon, const cv::Rect& roi)
{
  std::vector<cv::Point> result;
  result.reserve(polygon.size());
  for (const cv::Point& point : polygon)
  {
    result.emplace_back(point.x - roi.x, point.y - roi.y);
  }
  return result;
}

struct ThresholdMaskAnalysis
{
  cv::Point2d center;
  std::vector<cv::Point> pcaContour;
  cv::Rect boundingRect;
  cv::Mat componentMask;
  double pixelArea = 0.0;
  bool valid = false;
};

ThresholdMaskAnalysis analyzeLargestComponent(const cv::Mat& mask, const cv::Rect& roi)
{
  ThresholdMaskAnalysis analysis;

  cv::Mat labels;
  cv::Mat stats;
  cv::Mat centroids;
  const int componentCount =
    cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);

  int bestLabel = -1;
  int bestPixelArea = 0;
  for (int label = 1; label < componentCount; ++label)
  {
    const int pixelArea = stats.at<int>(label, cv::CC_STAT_AREA);
    if (pixelArea > bestPixelArea)
    {
      bestPixelArea = pixelArea;
      bestLabel = label;
    }
  }

  if (bestLabel < 0 || bestPixelArea <= 0)
  {
    return analysis;
  }

  analysis.pixelArea = static_cast<double>(bestPixelArea);
  analysis.boundingRect = cv::Rect(
    stats.at<int>(bestLabel, cv::CC_STAT_LEFT) + roi.x,
    stats.at<int>(bestLabel, cv::CC_STAT_TOP) + roi.y,
    stats.at<int>(bestLabel, cv::CC_STAT_WIDTH),
    stats.at<int>(bestLabel, cv::CC_STAT_HEIGHT));

  cv::Mat componentMask;
  cv::compare(labels, bestLabel, componentMask, cv::CMP_EQ);
  analysis.componentMask = componentMask;

  std::vector<std::vector<cv::Point>> componentContours;
  cv::findContours(componentMask.clone(), componentContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  double bestProfileArea = 0.0;
  for (const std::vector<cv::Point>& contour : componentContours)
  {
    if (contour.size() < 8)
    {
      continue;
    }

    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);
    const double profileArea = hull.size() >= 3 ? cv::contourArea(hull) : cv::contourArea(contour);
    if (profileArea <= bestProfileArea)
    {
      continue;
    }

    bestProfileArea = profileArea;
    analysis.pcaContour.clear();
    analysis.pcaContour.reserve(contour.size());
    for (const cv::Point& point : contour)
    {
      analysis.pcaContour.emplace_back(point.x + roi.x, point.y + roi.y);
    }
  }

  if (analysis.pcaContour.size() >= 3)
  {
    analysis.center = surfaceGeometricCenterFromContour(analysis.pcaContour);
  }
  else
  {
    const cv::Moments componentMoments = cv::moments(componentMask, true);
    if (componentMoments.m00 <= 0.0)
    {
      return analysis;
    }

    analysis.center = cv::Point2d(
      componentMoments.m10 / componentMoments.m00 + roi.x,
      componentMoments.m01 / componentMoments.m00 + roi.y);
  }

  analysis.valid = analysis.pcaContour.size() >= 5;
  return analysis;
}

void applyThresholdLocalization(
  SurfaceDefectResult& result,
  const cv::Mat& mask,
  const cv::Rect& roi,
  const SurfaceThresholdSettings& settings,
  bool createDiagnosticImage,
  bool drawContours)
{
  const ThresholdMaskAnalysis analysis = analyzeLargestComponent(mask, roi);
  if (!analysis.valid)
  {
    return;
  }

  SurfaceBlob mainBlob;
  mainBlob.area = analysis.pixelArea;
  mainBlob.center = analysis.center;
  mainBlob.boundingRect = analysis.boundingRect;
  mainBlob.contour = analysis.pcaContour;
  result.totalArea = analysis.pixelArea;
  result.blobs = {mainBlob};

  if (createDiagnosticImage)
  {
    drawSurfaceBlobs(result.diagnosticImage, result.blobs, drawContours);
  }

  result.localization.found = true;
  result.localization.method = "threshold_geometric_mass_pca";
  result.localization.center = mainBlob.center;
  result.localization.radius = mainBlob.area > 0.0 ? std::sqrt(mainBlob.area / CV_PI) : 0.0;
  result.localization.score = 1.0;
  result.localization.inputPoints = static_cast<int>(mainBlob.contour.size());
  result.localization.usedPoints = result.localization.inputPoints;

  cv::Point2d xDirection = surfaceLongAxisFromContour(mainBlob.contour);
  const cv::Point2d maskOrigin(roi.x, roi.y);
  int selectedInternalContourIndex = -1;

  if (settings.hasReferenceHalfPlaneSign())
  {
    xDirection = orientAxisToReferenceHalfPlane(
      mainBlob.center,
      xDirection,
      analysis.componentMask,
      maskOrigin,
      settings.referencePositiveHalfPlane);
  }
  else
  {
    const HalfPlaneMassResult halfPlane = orientAxisByHalfPlaneMass(
      mainBlob.center,
      xDirection,
      analysis.componentMask,
      maskOrigin);
    xDirection = halfPlane.axis;

    if (settings.resolveAmbiguity)
    {
      xDirection = orientAxisTowardMassAsymmetry(
        mainBlob.center,
        xDirection,
        analysis.componentMask,
        maskOrigin,
        mainBlob.boundingRect);
    }

    std::vector<std::vector<cv::Point>> imageContours;
    cv::findContours(analysis.componentMask.clone(), imageContours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    for (std::vector<cv::Point>& contour : imageContours)
    {
      for (cv::Point& point : contour)
      {
        point.x += roi.x;
        point.y += roi.y;
      }
    }

    if (settings.resolveAmbiguity && halfPlane.ambiguous())
    {
      xDirection = resolveSurfacePcaInternalAmbiguity(
        mainBlob.center,
        xDirection,
        imageContours,
        mainBlob.boundingRect,
        mainBlob.area,
        &selectedInternalContourIndex);
    }

    if (createDiagnosticImage && drawContours && selectedInternalContourIndex >= 0)
    {
      cv::drawContours(
        result.diagnosticImage,
        imageContours,
        selectedInternalContourIndex,
        cv::Scalar(0, 165, 255),
        2);
    }
  }

  const HalfPlaneMassResult finalHalfPlane = measureHalfPlaneMass(
    mainBlob.center,
    xDirection,
    analysis.componentMask,
    maskOrigin);
  result.localization.hasAxisReference = true;
  result.localization.referencePositiveHalfPlane =
    finalHalfPlane.positiveMass > finalHalfPlane.negativeMass;

  const double angle = std::atan2(xDirection.y, xDirection.x);

  const cv::Point2d xDir(std::cos(angle), std::sin(angle));
  const cv::Point2d yDir(-xDir.y, xDir.x);
  const cv::Rect bounding = mainBlob.boundingRect;
  const double axisLength = std::max(20.0, 0.55 * std::max(bounding.width, bounding.height));
  result.localization.angleRadians = angle;
  result.localization.xAxisStart = mainBlob.center - xDir * axisLength;
  result.localization.xAxisEnd = mainBlob.center + xDir * axisLength;
  result.localization.yAxisStart = mainBlob.center - yDir * axisLength;
  result.localization.yAxisEnd = mainBlob.center + yDir * axisLength;

  if (createDiagnosticImage)
  {
    drawStyledAxes(
      result.diagnosticImage,
      mainBlob.center,
      result.localization.xAxisStart,
      result.localization.xAxisEnd,
      result.localization.yAxisStart,
      result.localization.yAxisEnd);
  }
}
}

SurfaceDefectResult SurfaceThresholdStrategy::detectInRoi(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings,
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
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

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

  applyThresholdLocalization(result, mask, roi, settings, createDiagnosticImage, drawContours);

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfaceThresholdStrategy::detectInPolygon(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings,
  bool createDiagnosticImage,
  bool drawContours) const
{
  if (searchPolygon.size() < 3)
  {
    return detectInRoi(input, cv::Rect(), exclusionRects, settings);
  }

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

  const cv::Rect roi = clampSurfaceRect(cv::boundingRect(searchPolygon), gray.size());
  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  const int minValue = std::clamp(settings.minValue, 0, 255);
  const int maxValue = std::clamp(settings.maxValue, minValue, 255);
  cv::Mat mask;
  cv::inRange(gray(roi), cv::Scalar(minValue), cv::Scalar(maxValue), mask);

  cv::Mat polygonMask = cv::Mat::zeros(mask.size(), CV_8UC1);
  const std::vector<std::vector<cv::Point>> fillPolygons{localPolygon(searchPolygon, roi)};
  cv::fillPoly(polygonMask, fillPolygons, cv::Scalar(255));
  cv::bitwise_and(mask, polygonMask, mask);

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
  cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

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

  applyThresholdLocalization(result, mask, roi, settings, createDiagnosticImage, drawContours);
  if (result.localization.found)
  {
    result.localization.method = "threshold_polygon_mass_pca";
  }

  result.processed = true;
  return result;
}

SurfaceDefectResult SurfaceThresholdStrategy::locateAnnulus(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects,
  bool createDiagnosticImage,
  bool drawContours) const
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

  // Draw outer circle with shadow
  cv::circle(result.diagnosticImage, config.center, config.outerRadius, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
  cv::circle(result.diagnosticImage, config.center, config.outerRadius, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

  // Draw inner circle with shadow
  cv::circle(result.diagnosticImage, config.center, config.innerRadius, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
  cv::circle(result.diagnosticImage, config.center, config.innerRadius, cv::Scalar(0, 165, 255), 2, cv::LINE_AA);

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
  if (createDiagnosticImage)
  {
    drawSurfaceBlobs(result.diagnosticImage, result.blobs, drawContours);
  }
  if (!result.blobs.empty())
  {
    const SurfaceBlob& mainBlob = result.blobs.front();
    result.localization.found = true;
    result.localization.method = "threshold_annulus_center";
    result.localization.center = mainBlob.center;
    result.localization.radius = mainBlob.area > 0.0 ? std::sqrt(mainBlob.area / CV_PI) : 0.0;
    result.localization.score = 1.0;
    result.localization.inputPoints = static_cast<int>(mainBlob.contour.size());
    result.localization.usedPoints = result.localization.inputPoints;

    // Draw equivalent radius circle with shadow
    if (createDiagnosticImage && drawContours && result.localization.radius > 0.0)
    {
      const cv::Point centerPt = surfacePoint(mainBlob.center);
      const int radiusPt = static_cast<int>(std::round(result.localization.radius));
      cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
      cv::circle(result.diagnosticImage, centerPt, radiusPt, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
    }
  }

  result.processed = true;
  return result;
}
