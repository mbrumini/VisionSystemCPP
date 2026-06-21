#include "SurfaceModelStrategy.h"

#include "processing/ShapeCandidateFilter.h"
#include "processing/SurfaceProcessingUtils.h"

#include <opencv2/imgproc.hpp>

#include <limits>

namespace
{
cv::Mat toGray(const cv::Mat& input)
{
  if (input.channels() == 1)
  {
    return input;
  }

  cv::Mat gray;
  cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  return gray;
}

cv::Mat edgeImage(const cv::Mat& gray, int sensitivity)
{
  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);

  const int high = std::clamp(sensitivity, 1, 255);
  const int low = std::max(1, high / 2);
  cv::Mat edges;
  cv::Canny(blurred, edges, low, high);
  return edges;
}

void applyRoiAndExclusions(cv::Mat& mask, const cv::Rect& roi, const std::vector<cv::Rect>& exclusionRects)
{
  cv::Mat roiMask = cv::Mat::zeros(mask.size(), CV_8UC1);
  roiMask(roi).setTo(255);
  cv::bitwise_and(mask, roiMask, mask);

  for (const cv::Rect& exclusionRect : exclusionRects)
  {
    const cv::Rect clamped = clampSurfaceRect(exclusionRect, mask.size());
    if (!clamped.empty())
    {
      mask(clamped).setTo(0);
    }
  }
}

bool contourPose(const std::vector<cv::Point>& points, cv::Point2d& center, double& angleRadians)
{
  if (points.size() < 3)
  {
    return false;
  }

  cv::Mat data(static_cast<int>(points.size()), 2, CV_64F);
  for (int i = 0; i < static_cast<int>(points.size()); ++i)
  {
    data.at<double>(i, 0) = points[i].x;
    data.at<double>(i, 1) = points[i].y;
  }

  const cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);
  const cv::Moments moments = cv::moments(points);
  if (std::abs(moments.m00) > std::numeric_limits<double>::epsilon())
  {
    center = cv::Point2d(moments.m10 / moments.m00, moments.m01 / moments.m00);
  }
  else
  {
    center = cv::Point2d(pca.mean.at<double>(0, 0), pca.mean.at<double>(0, 1));
  }

  cv::Point2d e1(pca.eigenvectors.at<double>(0, 0), pca.eigenvectors.at<double>(0, 1));

  // Resolve 180-degree ambiguity using shape asymmetry
  const cv::Rect bounding = cv::boundingRect(points);
  const cv::Point2d boxCenter(bounding.x + bounding.width / 2.0, bounding.y + bounding.height / 2.0);
  const cv::Point2d asymmetryVec = center - boxCenter;
  const double dotProduct = asymmetryVec.x * e1.x + asymmetryVec.y * e1.y;
  if (dotProduct < 0.0)
  {
    e1 = -e1;
  }

  angleRadians = std::atan2(e1.y, e1.x);
  return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(angleRadians);
}

std::vector<cv::Point> transformedContour(
  const std::vector<cv::Point>& contour,
  const cv::Point2d& sourceCenter,
  double sourceAngle,
  const cv::Point2d& targetCenter,
  double targetAngle)
{
  std::vector<cv::Point> transformed;
  transformed.reserve(contour.size());
  const double angle = targetAngle - sourceAngle;
  const double cosine = std::cos(angle);
  const double sine = std::sin(angle);
  for (const cv::Point& point : contour)
  {
    const double x = point.x - sourceCenter.x;
    const double y = point.y - sourceCenter.y;
    transformed.emplace_back(
      cvRound(targetCenter.x + cosine * x - sine * y),
      cvRound(targetCenter.y + sine * x + cosine * y));
  }
  return transformed;
}

void fillReferenceAxes(SurfaceLocalizationReference& reference, const cv::Rect& bounds)
{
  const cv::Point2d xDirection(std::cos(reference.angleRadians), std::sin(reference.angleRadians));
  const cv::Point2d yDirection(-xDirection.y, xDirection.x);
  const double axisLength = std::max(20.0, 0.55 * std::max(bounds.width, bounds.height));
  reference.xAxisStart = reference.center - xDirection * axisLength;
  reference.xAxisEnd = reference.center + xDirection * axisLength;
  reference.yAxisStart = reference.center - yDirection * axisLength;
  reference.yAxisEnd = reference.center + yDirection * axisLength;
}

void fillReferenceAxesAtAngle(
  SurfaceLocalizationReference& reference,
  const cv::Rect& bounds,
  double drawingAngleRadians)
{
  const cv::Point2d xDirection(std::cos(drawingAngleRadians), std::sin(drawingAngleRadians));
  const cv::Point2d yDirection(-xDirection.y, xDirection.x);
  const double axisLength = std::max(20.0, 0.55 * std::max(bounds.width, bounds.height));
  reference.xAxisStart = reference.center - xDirection * axisLength;
  reference.xAxisEnd = reference.center + xDirection * axisLength;
  reference.yAxisStart = reference.center - yDirection * axisLength;
  reference.yAxisEnd = reference.center + yDirection * axisLength;
}

void drawReference(cv::Mat& image, const SurfaceLocalizationReference& reference)
{
  drawStyledCenterOfMass(image, reference.center);
  drawStyledAxes(image, reference.center, reference.xAxisStart, reference.xAxisEnd, reference.yAxisStart, reference.yAxisEnd);
}

struct RotatedTemplate
{
  cv::Mat image;
  cv::Point2d sourceCenter;
};

RotatedTemplate rotateTemplate(const cv::Mat& image, double angleDegrees)
{
  const cv::Point2f center((image.cols - 1) * 0.5F, (image.rows - 1) * 0.5F);
  cv::Mat rotation = cv::getRotationMatrix2D(center, angleDegrees, 1.0);
  const cv::Rect2f bounds = cv::RotatedRect(center, image.size(), static_cast<float>(angleDegrees)).boundingRect2f();
  rotation.at<double>(0, 2) += bounds.width * 0.5 - center.x;
  rotation.at<double>(1, 2) += bounds.height * 0.5 - center.y;

  cv::Mat rotated;
  cv::warpAffine(image, rotated, rotation, bounds.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));

  std::vector<cv::Point> nonZero;
  cv::findNonZero(rotated, nonZero);
  if (nonZero.empty())
  {
    return {};
  }

  const cv::Rect contentBounds = cv::boundingRect(nonZero);
  RotatedTemplate result;
  result.image = rotated(contentBounds).clone();
  result.sourceCenter = cv::Point2d(
    rotation.at<double>(0, 0) * center.x +
      rotation.at<double>(0, 1) * center.y +
      rotation.at<double>(0, 2) -
      contentBounds.x,
    rotation.at<double>(1, 0) * center.x +
      rotation.at<double>(1, 1) * center.y +
      rotation.at<double>(1, 2) -
      contentBounds.y);
  return result;
}
}

SurfaceDefectResult SurfaceModelStrategy::locateByShapeMatching(
  const cv::Mat& input,
  const SurfaceShapeMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  SurfaceDefectResult result;

  if (input.empty() || config.modelContour.size() < 3)
  {
    return result;
  }

  const cv::Mat gray = toGray(input);
  const cv::Rect roi = clampSurfaceRect(config.searchRoi, gray.size());
  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  cv::Mat edges = edgeImage(gray, config.edgeSensitivity);
  applyRoiAndExclusions(edges, roi, exclusionRects);
  cv::Mat roiEdges = edges(roi);
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
  cv::morphologyEx(roiEdges, roiEdges, cv::MORPH_CLOSE, kernel);
  cv::dilate(roiEdges, roiEdges, kernel);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(roiEdges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  for (auto& contour : contours)
  {
    for (auto& pt : contour)
    {
      pt.x += roi.x;
      pt.y += roi.y;
    }
  }
  result.localization.method = "shape_matching";
  result.localization.inputPoints = static_cast<int>(contours.size());

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::rectangle(result.diagnosticImage, roi, cv::Scalar(255, 0, 0), 2);

  double bestDistance = std::numeric_limits<double>::max();
  int bestIndex = -1;
  std::vector<cv::Point> bestCandidateContour;
  int acceptedCandidates = 0;
  const ShapeCandidateMetrics modelMetrics = measureShapeCandidate(config.modelContour);
  const double modelArea = modelMetrics.valid ? modelMetrics.area : std::abs(cv::contourArea(config.modelContour));
  const double minCandidateArea = std::max(config.minContourArea, modelArea * config.minAreaRatio);
  const double maxCandidateArea = std::max(minCandidateArea, modelArea * config.maxAreaRatio);

  std::vector<std::pair<double, int>> contourAreas;
  contourAreas.reserve(contours.size());
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    contourAreas.emplace_back(std::abs(cv::contourArea(contours[i])), i);
  }
  std::sort(contourAreas.begin(), contourAreas.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });

  const int maxCandidatesToTest = std::min(10, static_cast<int>(contourAreas.size()));
  for (int idx = 0; idx < maxCandidatesToTest; ++idx)
  {
    int i = contourAreas[idx].second;
    std::vector<cv::Point> candidateContour;
    if (config.useConvexHull)
    {
      const double anchorArea = std::abs(cv::contourArea(contours[i]));
      if (anchorArea < config.minContourArea)
      {
        continue;
      }

      cv::Moments m = cv::moments(contours[i]);
      cv::Point2d center;
      if (std::abs(m.m00) > std::numeric_limits<double>::epsilon())
      {
        center = cv::Point2d(m.m10 / m.m00, m.m01 / m.m00);
      }
      else
      {
        cv::Rect bounding = cv::boundingRect(contours[i]);
        center = cv::Point2d(bounding.x + bounding.width / 2.0, bounding.y + bounding.height / 2.0);
      }

      cv::Rect modelBounds = cv::boundingRect(config.modelContour);
      double diagonal = std::sqrt(modelBounds.width * modelBounds.width + modelBounds.height * modelBounds.height);
      double windowSize = diagonal * 1.35;

      cv::Rect localWindow(
        static_cast<int>(center.x - windowSize / 2.0),
        static_cast<int>(center.y - windowSize / 2.0),
        static_cast<int>(windowSize),
        static_cast<int>(windowSize));
      localWindow = clampSurfaceRect(localWindow, edges.size());

      if (localWindow.width < 4 || localWindow.height < 4)
      {
        continue;
      }

      std::vector<cv::Point> localEdgePixels;
      cv::findNonZero(edges(localWindow), localEdgePixels);
      if (localEdgePixels.size() < 3)
      {
        continue;
      }

      for (auto& pt : localEdgePixels)
      {
        pt.x += localWindow.x;
        pt.y += localWindow.y;
      }

      cv::convexHull(localEdgePixels, candidateContour);
    }
    else
    {
      candidateContour = contours[i];
    }

    const ShapeCandidateMetrics candidateMetrics = measureShapeCandidate(candidateContour);
    if (!candidateMetrics.valid ||
        candidateMetrics.area < minCandidateArea ||
        candidateMetrics.area > maxCandidateArea)
    {
      continue;
    }

    if (modelMetrics.valid && !acceptsShapeCandidate(modelMetrics, candidateMetrics))
    {
      continue;
    }

    ++acceptedCandidates;
    const double distance = cv::matchShapes(config.modelContour, candidateContour, cv::CONTOURS_MATCH_I1, 0.0);
    if (distance < bestDistance)
    {
      bestDistance = distance;
      bestIndex = i;
      bestCandidateContour = candidateContour;
    }
  }

  result.processed = true;
  result.localization.usedPoints = acceptedCandidates;
  result.localization.meanError =
    std::isfinite(bestDistance) ? bestDistance : -1.0;
  result.localization.score =
    std::isfinite(bestDistance) ? 1.0 / (1.0 + bestDistance) : 0.0;
  if (bestIndex < 0 || bestDistance > config.maxShapeDistance)
  {
    return result;
  }

  SurfaceBlob blob;
  blob.contour = bestCandidateContour;
  blob.area = std::abs(cv::contourArea(blob.contour));
  blob.boundingRect = cv::boundingRect(blob.contour);
  if (!contourPose(blob.contour, blob.center, result.localization.angleRadians))
  {
    return result;
  }

  result.totalArea = blob.area;
  result.blobs.push_back(blob);
  result.localization.found = true;
  result.localization.center = blob.center;
  fillReferenceAxes(result.localization, blob.boundingRect);

  cv::Mat detectedArea = result.diagnosticImage.clone();
  cv::drawContours(
    detectedArea,
    std::vector<std::vector<cv::Point>>{blob.contour},
    0,
    cv::Scalar(0, 210, 0),
    cv::FILLED);
  cv::addWeighted(detectedArea, 0.22, result.diagnosticImage, 0.78, 0.0, result.diagnosticImage);
  cv::drawContours(
    result.diagnosticImage,
    std::vector<std::vector<cv::Point>>{blob.contour},
    0,
    cv::Scalar(0, 255, 0),
    2);

  cv::Point2d modelCenter;
  double modelAngle = 0.0;
  if (contourPose(config.modelContour, modelCenter, modelAngle))
  {
    const std::vector<cv::Point> posedModel = transformedContour(
      config.modelContour,
      modelCenter,
      modelAngle,
      blob.center,
      result.localization.angleRadians);
    drawStyledContour(result.diagnosticImage, posedModel, cv::Scalar(0, 255, 255));
  }
  // cv::rectangle(result.diagnosticImage, blob.boundingRect, cv::Scalar(255, 0, 0), 2);
  drawReference(result.diagnosticImage, result.localization);
  return result;
}

SurfaceDefectResult SurfaceModelStrategy::locateByTemplateMatching(
  const cv::Mat& input,
  const SurfaceTemplateMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  SurfaceDefectResult result;

  if (input.empty() || config.modelImage.empty())
  {
    return result;
  }

  const cv::Mat gray = toGray(input);
  const cv::Rect roi = clampSurfaceRect(config.searchRoi, gray.size());
  if (roi.width < 4 || roi.height < 4)
  {
    return result;
  }

  cv::Mat search;
  cv::Mat modelToMatch;
  if (config.useEdges)
  {
    cv::Mat searchEdges = edgeImage(gray, config.edgeSensitivity);
    applyRoiAndExclusions(searchEdges, roi, exclusionRects);
    search = searchEdges(roi);
    modelToMatch = edgeImage(toGray(config.modelImage), config.edgeSensitivity);
  }
  else
  {
    cv::Mat searchGray = gray.clone();
    applyRoiAndExclusions(searchGray, roi, exclusionRects);
    search = searchGray(roi);
    modelToMatch = toGray(config.modelImage);
  }

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  // cv::rectangle(result.diagnosticImage, roi, cv::Scalar(255, 0, 0), 2);

  double bestScore = -1.0;
  double bestAngle = 0.0;
  cv::Point bestLocation;
  cv::Size bestSize;
  cv::Point2d bestSourceCenter;
  const double step = std::max(1.0, std::abs(config.angleStepDegrees));

  // Determine scale factor dynamically based on size
  int scale = 1;
  if (search.cols >= 800 || search.rows >= 800)
  {
    scale = 4;
  }
  else if (search.cols >= 300 || search.rows >= 300)
  {
    scale = 2;
  }

  // Ensure downscaled model image is not too small
  if (scale > 1)
  {
    if (modelToMatch.cols / scale < 10 || modelToMatch.rows / scale < 10)
    {
      scale = 1;
    }
  }

  if (scale > 1)
  {
    cv::Mat searchSmall;
    cv::Mat modelSmall;
    cv::resize(search, searchSmall, cv::Size(), 1.0 / scale, 1.0 / scale, cv::INTER_LINEAR);
    cv::resize(modelToMatch, modelSmall, cv::Size(), 1.0 / scale, 1.0 / scale, cv::INTER_LINEAR);

    double coarseBestScore = -1.0;
    double coarseBestAngle = 0.0;
    cv::Point coarseBestLocationSmall;
    cv::Size coarseBestSizeSmall;
    cv::Point2d coarseBestSourceCenterSmall;

    auto evaluateAngleSmall = [&](double angle) {
      const RotatedTemplate rotatedTemplate = rotateTemplate(modelSmall, angle);
      if (rotatedTemplate.image.cols < 4 || rotatedTemplate.image.rows < 4 ||
          rotatedTemplate.image.cols > searchSmall.cols || rotatedTemplate.image.rows > searchSmall.rows)
      {
        return;
      }

      cv::Mat response;
      cv::matchTemplate(searchSmall, rotatedTemplate.image, response, cv::TM_CCOEFF_NORMED);

      double minValue = 0.0;
      double maxValue = 0.0;
      cv::Point minLocation;
      cv::Point maxLocation;
      cv::minMaxLoc(response, &minValue, &maxValue, &minLocation, &maxLocation);

      if (maxValue > coarseBestScore)
      {
        coarseBestScore = maxValue;
        coarseBestAngle = angle;
        coarseBestLocationSmall = maxLocation;
        coarseBestSizeSmall = rotatedTemplate.image.size();
        coarseBestSourceCenterSmall = rotatedTemplate.sourceCenter;
      }
    };

    // Coarse search on downscaled images
    for (double angle = config.angleStartDegrees; angle <= config.angleEndDegrees; angle += step)
    {
      evaluateAngleSmall(angle);
    }

    if (coarseBestScore >= 0.0 && !coarseBestSizeSmall.empty())
    {
      // Refine on full-resolution search image around the coarse location
      const double coarseBestAngleVal = coarseBestAngle;
      const double refineStart = std::max(config.angleStartDegrees, coarseBestAngleVal - step);
      const double refineEnd = std::min(config.angleEndDegrees, coarseBestAngleVal + step);

      const cv::Point coarseBestLocation(coarseBestLocationSmall.x * scale, coarseBestLocationSmall.y * scale);
      const int pad = scale * 2 + 4;

      auto evaluateAngleFull = [&](double angle) {
        const RotatedTemplate rotatedTemplate = rotateTemplate(modelToMatch, angle);
        if (rotatedTemplate.image.cols < 4 || rotatedTemplate.image.rows < 4)
        {
          return;
        }

        // Local search area centered at the coarse match location
        cv::Rect localSearchRoi(
          coarseBestLocation.x - pad,
          coarseBestLocation.y - pad,
          rotatedTemplate.image.cols + 2 * pad,
          rotatedTemplate.image.rows + 2 * pad
        );

        // Clamp to search boundaries
        int x1 = std::clamp(localSearchRoi.x, 0, search.cols);
        int y1 = std::clamp(localSearchRoi.y, 0, search.rows);
        int x2 = std::clamp(localSearchRoi.x + localSearchRoi.width, 0, search.cols);
        int y2 = std::clamp(localSearchRoi.y + localSearchRoi.height, 0, search.rows);

        if (x2 - x1 < rotatedTemplate.image.cols || y2 - y1 < rotatedTemplate.image.rows)
        {
          return;
        }
        cv::Rect clampedLocalSearchRoi(x1, y1, x2 - x1, y2 - y1);
        cv::Mat searchLocal = search(clampedLocalSearchRoi);

        cv::Mat response;
        cv::matchTemplate(searchLocal, rotatedTemplate.image, response, cv::TM_CCOEFF_NORMED);

        double minValue = 0.0;
        double maxValue = 0.0;
        cv::Point minLocation;
        cv::Point maxLocation;
        cv::minMaxLoc(response, &minValue, &maxValue, &minLocation, &maxLocation);

        if (maxValue > bestScore)
        {
          bestScore = maxValue;
          bestAngle = angle;
          bestLocation = cv::Point(clampedLocalSearchRoi.x + maxLocation.x, clampedLocalSearchRoi.y + maxLocation.y);
          bestSize = rotatedTemplate.image.size();
          bestSourceCenter = rotatedTemplate.sourceCenter;
        }
      };

      for (double angle = refineStart; angle <= refineEnd; angle += 1.0)
      {
        evaluateAngleFull(angle);
      }
    }
  }
  else
  {
    // Fallback: standard search on original scale
    auto evaluateAngle = [&](double angle) {
      const RotatedTemplate rotatedTemplate = rotateTemplate(modelToMatch, angle);
      if (rotatedTemplate.image.cols < 4 || rotatedTemplate.image.rows < 4 ||
          rotatedTemplate.image.cols > search.cols || rotatedTemplate.image.rows > search.rows)
      {
        return;
      }

      cv::Mat response;
      cv::matchTemplate(search, rotatedTemplate.image, response, cv::TM_CCOEFF_NORMED);

      double minValue = 0.0;
      double maxValue = 0.0;
      cv::Point minLocation;
      cv::Point maxLocation;
      cv::minMaxLoc(response, &minValue, &maxValue, &minLocation, &maxLocation);

      if (maxValue > bestScore)
      {
        bestScore = maxValue;
        bestAngle = angle;
        bestLocation = maxLocation;
        bestSize = rotatedTemplate.image.size();
        bestSourceCenter = rotatedTemplate.sourceCenter;
      }
    };

    for (double angle = config.angleStartDegrees; angle <= config.angleEndDegrees; angle += step)
    {
      evaluateAngle(angle);
    }

    if (bestScore >= 0.0 && step > 1.0)
    {
      const double coarseBestAngle = bestAngle;
      const double refineStart = std::max(config.angleStartDegrees, coarseBestAngle - step);
      const double refineEnd = std::min(config.angleEndDegrees, coarseBestAngle + step);
      for (double angle = refineStart; angle <= refineEnd; angle += 1.0)
      {
        evaluateAngle(angle);
      }
    }
  }

  result.processed = true;
  if (bestScore < config.minScore || bestSize.empty())
  {
    return result;
  }

  const cv::Rect matchRect(roi.x + bestLocation.x, roi.y + bestLocation.y, bestSize.width, bestSize.height);
  const cv::Point2d center(
    roi.x + bestLocation.x + bestSourceCenter.x,
    roi.y + bestLocation.y + bestSourceCenter.y);

  SurfaceBlob blob;
  blob.area = static_cast<double>(matchRect.area());
  blob.center = center;
  blob.boundingRect = matchRect;

  result.totalArea = blob.area;
  result.blobs.push_back(blob);
  result.localization.found = true;
  result.localization.method = "template_matching";
  result.localization.center = center;
  result.localization.angleRadians = bestAngle * CV_PI / 180.0;
  result.localization.score = bestScore;
  result.localization.usedPoints = 1;
  result.localization.inputPoints = static_cast<int>((config.angleEndDegrees - config.angleStartDegrees) / step) + 1;
  // getRotationMatrix2D uses mathematical positive angles while image Y grows
  // downwards. Keep the reported test angle positive, but draw the image axes
  // with the corresponding visual rotation.
  fillReferenceAxesAtAngle(
    result.localization,
    matchRect,
    -result.localization.angleRadians);

  cv::rectangle(result.diagnosticImage, matchRect, cv::Scalar(0, 255, 0), 2);
  drawReference(result.diagnosticImage, result.localization);
  return result;
}
