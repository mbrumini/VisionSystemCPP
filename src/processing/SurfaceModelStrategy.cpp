#include "SurfaceModelStrategy.h"

#include "processing/ShapeCandidateFilter.h"
#include "processing/SurfaceProcessingUtils.h"

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <limits>
#include <vector>

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
  if (isElongatedContour(points))
  {
    cv::Point2d xDirection;
    cv::Point2d yDirection;
    assignLocalizationAxesLongSideOnY(angleRadians, xDirection, yDirection, points);
    angleRadians = std::atan2(xDirection.y, xDirection.x);
  }
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

  double borderVal = 0.0;
  if (!image.empty())
  {
    borderVal = static_cast<double>(image.at<uchar>(0, 0));
  }
  cv::Mat rotated;
  cv::warpAffine(image, rotated, rotation, bounds.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(borderVal));

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

struct TemplateAngleMatch
{
  double score = -1.0;
  double angle = 0.0;
  cv::Point location;
  cv::Size size;
  cv::Point2d sourceCenter;

  bool valid() const { return score >= 0.0 && !size.empty(); }
};

struct AngleStepPlan
{
  double coarseStep = 5.0;
  double refineStep = 1.0;
};

AngleStepPlan planAngleSteps(double angleStart, double angleEnd, double requestedStep)
{
  AngleStepPlan plan;
  const double span = std::max(1.0, angleEnd - angleStart);
  constexpr int kMaxCoarseAngles = 32;
  plan.coarseStep = std::max(requestedStep, std::ceil(span / static_cast<double>(kMaxCoarseAngles)));
  plan.refineStep = std::max(1.0, std::min(requestedStep, plan.coarseStep * 0.5));
  return plan;
}

struct AngleSearchPhase
{
  double start = 0.0;
  double end = 0.0;
};

std::vector<AngleSearchPhase> buildAngleSearchPhases(
  double fullStart,
  double fullEnd,
  bool hasReferenceAngle,
  double referenceAngleDegrees)
{
  const double span = fullEnd - fullStart;
  if (!hasReferenceAngle || span <= 90.0)
  {
    return {{fullStart, fullEnd}};
  }

  const double margin = std::min(45.0, span * 0.125);
  const double narrowStart = std::max(fullStart, referenceAngleDegrees - margin);
  const double narrowEnd = std::min(fullEnd, referenceAngleDegrees + margin);
  if (narrowEnd - narrowStart < planAngleSteps(narrowStart, narrowEnd, 1.0).coarseStep)
  {
    return {{fullStart, fullEnd}};
  }

  return {{narrowStart, narrowEnd}, {fullStart, fullEnd}};
}

std::vector<cv::Mat> buildGaussianPyramid(const cv::Mat& image, int maxLevels = 4, int minSide = 32)
{
  std::vector<cv::Mat> pyramid;
  pyramid.push_back(image);
  while (static_cast<int>(pyramid.size()) < maxLevels)
  {
    const cv::Mat& current = pyramid.back();
    if (current.cols < minSide || current.rows < minSide)
    {
      break;
    }

    cv::Mat next;
    cv::pyrDown(current, next);
    if (next.cols < 4 || next.rows < 4)
    {
      break;
    }
    pyramid.push_back(next);
  }
  return pyramid;
}

bool evaluateTemplateAtLevel(
  const cv::Mat& searchLevel,
  const cv::Mat& modelLevel,
  double angle,
  const cv::Point* locationHint,
  int localPad,
  TemplateAngleMatch& best)
{
  const RotatedTemplate rotatedTemplate = rotateTemplate(modelLevel, angle);
  if (rotatedTemplate.image.cols < 4 || rotatedTemplate.image.rows < 4)
  {
    return false;
  }

  cv::Mat searchRegion;
  cv::Point regionOrigin(0, 0);

  if (locationHint != nullptr)
  {
    cv::Rect localSearchRoi(
      locationHint->x - localPad,
      locationHint->y - localPad,
      rotatedTemplate.image.cols + 2 * localPad,
      rotatedTemplate.image.rows + 2 * localPad);

    const int x1 = std::clamp(localSearchRoi.x, 0, searchLevel.cols);
    const int y1 = std::clamp(localSearchRoi.y, 0, searchLevel.rows);
    const int x2 = std::clamp(localSearchRoi.x + localSearchRoi.width, 0, searchLevel.cols);
    const int y2 = std::clamp(localSearchRoi.y + localSearchRoi.height, 0, searchLevel.rows);

    if (x2 - x1 < rotatedTemplate.image.cols || y2 - y1 < rotatedTemplate.image.rows)
    {
      return false;
    }

    const cv::Rect clampedRoi(x1, y1, x2 - x1, y2 - y1);
    searchRegion = searchLevel(clampedRoi);
    regionOrigin = clampedRoi.tl();
  }
  else
  {
    if (rotatedTemplate.image.cols > searchLevel.cols || rotatedTemplate.image.rows > searchLevel.rows)
    {
      return false;
    }
    searchRegion = searchLevel;
  }

  cv::Mat response;
  cv::matchTemplate(searchRegion, rotatedTemplate.image, response, cv::TM_CCOEFF_NORMED);

  double minValue = 0.0;
  double maxValue = 0.0;
  cv::Point minLocation;
  cv::Point maxLocation;
  cv::minMaxLoc(response, &minValue, &maxValue, &minLocation, &maxLocation);

  if (maxValue > best.score)
  {
    best.score = maxValue;
    best.angle = angle;
    best.location = regionOrigin + maxLocation;
    best.size = rotatedTemplate.image.size();
    best.sourceCenter = rotatedTemplate.sourceCenter;
    return true;
  }

  return false;
}

TemplateAngleMatch runPyramidTemplateAngleSearch(
  const cv::Mat& search,
  const cv::Mat& modelToMatch,
  double angleStart,
  double angleEnd,
  const AngleStepPlan& plan)
{
  TemplateAngleMatch best;
  const double coarseStep = plan.coarseStep;
  const double refineStep = plan.refineStep;

  const std::vector<cv::Mat> searchPyramid = buildGaussianPyramid(search);
  const std::vector<cv::Mat> modelPyramid = buildGaussianPyramid(modelToMatch);
  const int topLevel = static_cast<int>(searchPyramid.size()) - 1;

  for (double angle = angleStart; angle <= angleEnd; angle += coarseStep)
  {
    evaluateTemplateAtLevel(searchPyramid[topLevel], modelPyramid[topLevel], angle, nullptr, 0, best);
  }

  if (!best.valid())
  {
    return best;
  }

  if (topLevel == 0)
  {
    TemplateAngleMatch refineBest;
    refineBest.score = -1.0;
    const double refineStart = std::max(angleStart, best.angle - coarseStep);
    const double refineEnd = std::min(angleEnd, best.angle + coarseStep);
    for (double angle = refineStart; angle <= refineEnd; angle += refineStep)
    {
      evaluateTemplateAtLevel(searchPyramid[0], modelPyramid[0], angle, &best.location, coarseStep + 4, refineBest);
    }
    if (refineBest.valid())
    {
      best = refineBest;
    }
    return best;
  }

  cv::Point locationHint = best.location;
  double angleHint = best.angle;

  for (int level = topLevel - 1; level >= 0; --level)
  {
    locationHint.x = best.location.x * 2;
    locationHint.y = best.location.y * 2;
    angleHint = best.angle;

    const int levelsAbove = topLevel - level;
    const double angleWindow = coarseStep / static_cast<double>(levelsAbove + 1);
    const double levelStep = (level == 0) ? refineStep : std::max(refineStep, coarseStep * 0.5);
    const int localPad = 6 + (1 << levelsAbove);

    TemplateAngleMatch levelBest;
    levelBest.score = -1.0;

    const double refineStart = std::max(angleStart, angleHint - angleWindow);
    const double refineEnd = std::min(angleEnd, angleHint + angleWindow);
    for (double angle = refineStart; angle <= refineEnd; angle += levelStep)
    {
      evaluateTemplateAtLevel(searchPyramid[level], modelPyramid[level], angle, &locationHint, localPad, levelBest);
    }

    if (levelBest.valid())
    {
      best = levelBest;
    }
    else
    {
      best.location = locationHint;
    }
  }

  if (topLevel > 0 && best.valid())
  {
    const int finalPad = static_cast<int>(std::ceil(coarseStep)) + 4;
    TemplateAngleMatch finalBest;
    finalBest.score = -1.0;
    if (evaluateTemplateAtLevel(searchPyramid[0], modelPyramid[0], best.angle, &best.location, finalPad, finalBest))
    {
      best = finalBest;
    }
  }

  return best;
}

TemplateAngleMatch runTemplateAngleSearch(
  const cv::Mat& search,
  const cv::Mat& modelToMatch,
  double angleStart,
  double angleEnd,
  const AngleStepPlan& plan)
{
  return runPyramidTemplateAngleSearch(search, modelToMatch, angleStart, angleEnd, plan);
}
}

SurfaceDefectResult SurfaceModelStrategy::locateByShapeMatching(
  const cv::Mat& input,
  const SurfaceShapeMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects,
  bool createDiagnosticImage,
  bool drawContours) const
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

  if (createDiagnosticImage)
  {
    if (drawContours)
    {
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
    }

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
      if (drawContours)
      {
        drawStyledContour(result.diagnosticImage, posedModel, cv::Scalar(0, 255, 255));
      }
    }
    // cv::rectangle(result.diagnosticImage, blob.boundingRect, cv::Scalar(255, 0, 0), 2);
    drawReference(result.diagnosticImage, result.localization);
  }
  return result;
}

SurfaceDefectResult SurfaceModelStrategy::locateByTemplateMatching(
  const cv::Mat& input,
  const SurfaceTemplateMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects,
  bool createDiagnosticImage,
  bool drawContours) const
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

  double bestScore = -1.0;
  double bestAngle = 0.0;
  cv::Point bestLocation;
  cv::Size bestSize;
  cv::Point2d bestSourceCenter;
  const double requestedStep = std::max(1.0, std::abs(config.angleStepDegrees));
  int angleEvaluations = 0;

  const auto phases = buildAngleSearchPhases(
    config.angleStartDegrees,
    config.angleEndDegrees,
    config.hasReferenceAngle,
    config.referenceAngleDegrees);

  for (const AngleSearchPhase& phase : phases)
  {
    const AngleStepPlan plan = planAngleSteps(phase.start, phase.end, requestedStep);
    angleEvaluations += static_cast<int>((phase.end - phase.start) / plan.coarseStep) + 1;

    const TemplateAngleMatch match =
      runTemplateAngleSearch(search, modelToMatch, phase.start, phase.end, plan);
    if (match.valid() && match.score > bestScore)
    {
      bestScore = match.score;
      bestAngle = match.angle;
      bestLocation = match.location;
      bestSize = match.size;
      bestSourceCenter = match.sourceCenter;
    }

    if (bestScore >= config.minScore)
    {
      break;
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
  result.localization.inputPoints = angleEvaluations;
  // getRotationMatrix2D uses mathematical positive angles while image Y grows
  // downwards. Keep the reported test angle positive, but draw the image axes
  // with the corresponding visual rotation.
  fillReferenceAxesAtAngle(
    result.localization,
    matchRect,
    -result.localization.angleRadians);

  if (createDiagnosticImage)
  {
    if (drawContours)
    {
      cv::rectangle(result.diagnosticImage, matchRect, cv::Scalar(0, 255, 0), 2);
    }
    drawReference(result.diagnosticImage, result.localization);
  }
  return result;
}
