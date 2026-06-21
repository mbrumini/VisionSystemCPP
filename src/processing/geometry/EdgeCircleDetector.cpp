#include "EdgeCircleDetector.h"

#include "processing/CircleFit.h"
#include "processing/SurfaceProcessingUtils.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
bool pointInsideImage(const cv::Mat& image, const cv::Point2d& point)
{
  return point.x >= 0.0 && point.y >= 0.0 && point.x < image.cols && point.y < image.rows;
}

int sampledGray(const cv::Mat& gray, const cv::Point2d& point)
{
  const int x = std::clamp(static_cast<int>(std::round(point.x)), 0, gray.cols - 1);
  const int y = std::clamp(static_cast<int>(std::round(point.y)), 0, gray.rows - 1);
  return static_cast<int>(gray.at<uchar>(y, x));
}

cv::Point2d interpolatedCandidate(
  const cv::Point2d& previousPoint,
  const cv::Point2d& currentPoint,
  int previousValue,
  int currentValue,
  bool useSubpixel)
{
  if (!useSubpixel || previousValue == currentValue)
  {
    return currentPoint;
  }

  const double target = (static_cast<double>(previousValue) + static_cast<double>(currentValue)) * 0.5;
  const double t = std::clamp(
    (target - static_cast<double>(previousValue)) /
      (static_cast<double>(currentValue) - static_cast<double>(previousValue)),
    0.0,
    1.0);
  return previousPoint + (currentPoint - previousPoint) * t;
}

std::vector<cv::Point2d> scanCircleEdges(const cv::Mat& gray, const EdgeCircleDetectorConfig& config, int gradientThreshold)
{
  std::vector<cv::Point2d> points;
  if (config.guideRadius <= 1.0 || config.innerBand <= 0 || config.outerBand <= 0 ||
      config.guideRadius <= static_cast<double>(config.innerBand))
  {
    return points;
  }

  double startAngle = 0.0;
  double span = 2.0 * CV_PI;
  if (config.useArc)
  {
    startAngle = config.startAngleRadians;
    span = config.endAngleRadians - config.startAngleRadians;
    while (span < 0.0)
    {
      span += 2.0 * CV_PI;
    }
    if (span < 0.001)
    {
      span = 2.0 * CV_PI;
    }
  }

  const int fullCircleSamples = std::max(90, static_cast<int>(std::round(config.guideRadius * 2.0)));
  const int angleSamples = config.useArc
    ? std::max(12, static_cast<int>(std::round(fullCircleSamples * span / (2.0 * CV_PI))))
    : fullCircleSamples;
  const bool outwardScan = config.scanDirection == EdgeLineScanDirection::NormalPositive;
  const double startOffset = outwardScan ? -static_cast<double>(config.innerBand) : static_cast<double>(config.outerBand);
  const double endOffset = outwardScan ? static_cast<double>(config.outerBand) : -static_cast<double>(config.innerBand);

  for (int i = 0; i < angleSamples; ++i)
  {
    const double angle = startAngle + span * static_cast<double>(i) / static_cast<double>(std::max(1, angleSamples - 1));
    const cv::Point2d radial(std::cos(angle), std::sin(angle));
    cv::Point2d previousPoint = config.guideCenter + radial * (config.guideRadius + startOffset);
    if (!pointInsideImage(gray, previousPoint))
    {
      continue;
    }

    int previousValue = sampledGray(gray, previousPoint);
    bool found = false;
    int bestStrength = -1;
    cv::Point2d bestPoint;
    const int steps = std::max(2, config.innerBand + config.outerBand);
    for (int step = 1; step <= steps; ++step)
    {
      const double offset = startOffset + (endOffset - startOffset) * static_cast<double>(step) / static_cast<double>(steps);
      const cv::Point2d currentPoint = config.guideCenter + radial * (config.guideRadius + offset);
      if (!pointInsideImage(gray, currentPoint))
      {
        previousPoint = currentPoint;
        continue;
      }

      const int currentValue = sampledGray(gray, currentPoint);
      const int delta = currentValue - previousValue;
      const bool matchesTransition =
        (config.transition == EdgeLineTransition::DarkToLight && delta >= gradientThreshold) ||
        (config.transition == EdgeLineTransition::LightToDark && delta <= -gradientThreshold);
      if (matchesTransition)
      {
        const cv::Point2d candidate = interpolatedCandidate(
          previousPoint,
          currentPoint,
          previousValue,
          currentValue,
          config.useSubpixel);
        const int strength = std::abs(delta);
        if (config.pickMode == EdgeLinePickMode::First)
        {
          points.push_back(candidate);
          found = true;
          break;
        }

        if (config.pickMode == EdgeLinePickMode::Last || strength > bestStrength)
        {
          bestStrength = strength;
          bestPoint = candidate;
          found = true;
        }
      }

      previousPoint = currentPoint;
      previousValue = currentValue;
    }

    if (found && config.pickMode != EdgeLinePickMode::First)
    {
      points.push_back(bestPoint);
    }
  }

  return points;
}

double configuredArcSpanRadians(const EdgeCircleDetectorConfig& config)
{
  if (!config.useArc)
  {
    return 2.0 * CV_PI;
  }

  double span = config.endAngleRadians - config.startAngleRadians;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  return span < 0.001 ? 2.0 * CV_PI : span;
}

std::vector<cv::Point2d> filterByRadialDerivative(const std::vector<cv::Point2d>& points, const EdgeCircleDetectorConfig& config)
{
  if (points.size() < 3 || config.edgeCleanupDerivative <= 0)
  {
    return points;
  }

  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  filtered.push_back(points.front());
  for (int i = 1; i < static_cast<int>(points.size()) - 1; ++i)
  {
    const double previousRadius = cv::norm(points[i - 1] - config.guideCenter);
    const double radius = cv::norm(points[i] - config.guideCenter);
    const double nextRadius = cv::norm(points[i + 1] - config.guideCenter);
    if (std::min(std::abs(radius - previousRadius), std::abs(nextRadius - radius)) <= config.edgeCleanupDerivative)
    {
      filtered.push_back(points[i]);
    }
  }
  filtered.push_back(points.back());
  return filtered;
}

std::vector<cv::Point2d> filterByRadialMedianDeviation(const std::vector<cv::Point2d>& points, const EdgeCircleDetectorConfig& config)
{
  if (points.size() < 3 || config.edgeStatisticalFilter <= 0)
  {
    return points;
  }

  std::vector<double> radii;
  radii.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    radii.push_back(cv::norm(point - config.guideCenter));
  }
  std::sort(radii.begin(), radii.end());
  const double median = radii[radii.size() / 2];

  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    if (std::abs(cv::norm(point - config.guideCenter) - median) <= config.edgeStatisticalFilter)
    {
      filtered.push_back(point);
    }
  }
  return filtered;
}

std::vector<cv::Point> toIntegerPoints(const std::vector<cv::Point2d>& points)
{
  std::vector<cv::Point> result;
  result.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    result.push_back(surfacePoint(point));
  }
  return result;
}
}

EdgeCircleDetectorResult EdgeCircleDetector::detect(const cv::Mat& input, const EdgeCircleDetectorConfig& config) const
{
  EdgeCircleDetectorResult result;
  if (input.empty())
  {
    result.message = "Input cerchio edge non valido";
    return result;
  }

  cv::Mat gray;
  if (input.channels() == 1)
  {
    gray = input;
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    input.copyTo(result.diagnosticImage);
  }

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0.0);
  const int sensitivity = std::clamp(config.edgeSensitivity, 1, 255);
  const int gradientThreshold = std::max(2, (256 - sensitivity) / 12 + 1);
  result.rawEdgePoints = scanCircleEdges(blurred, config, gradientThreshold);
  result.edgePoints = filterByRadialDerivative(result.rawEdgePoints, config);
  result.edgePoints = filterByRadialMedianDeviation(result.edgePoints, config);

  result.processed = true;
  if (config.guideRadius <= static_cast<double>(config.innerBand))
  {
    result.message = "Banda interna cerchio edge non valida";
    return result;
  }

  const double startDegrees = config.startAngleRadians * 180.0 / CV_PI;
  double endDegrees = config.endAngleRadians * 180.0 / CV_PI;
  if (config.useArc && endDegrees < startDegrees)
  {
    endDegrees += 360.0;
  }
  if (config.useArc)
  {
    cv::ellipse(result.diagnosticImage, surfacePoint(config.guideCenter),
      cv::Size(static_cast<int>(std::round(config.guideRadius)), static_cast<int>(std::round(config.guideRadius))),
      0.0, startDegrees, endDegrees, cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
    cv::ellipse(result.diagnosticImage, surfacePoint(config.guideCenter),
      cv::Size(std::max(1, static_cast<int>(std::round(config.guideRadius - config.innerBand))), std::max(1, static_cast<int>(std::round(config.guideRadius - config.innerBand)))),
      0.0, startDegrees, endDegrees, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    cv::ellipse(result.diagnosticImage, surfacePoint(config.guideCenter),
      cv::Size(static_cast<int>(std::round(config.guideRadius + config.outerBand)), static_cast<int>(std::round(config.guideRadius + config.outerBand))),
      0.0, startDegrees, endDegrees, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
  }
  else
  {
    cv::circle(result.diagnosticImage, surfacePoint(config.guideCenter), static_cast<int>(std::round(config.guideRadius)), cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
    cv::circle(result.diagnosticImage, surfacePoint(config.guideCenter), static_cast<int>(std::round(config.guideRadius - config.innerBand)), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    cv::circle(result.diagnosticImage, surfacePoint(config.guideCenter), static_cast<int>(std::round(config.guideRadius + config.outerBand)), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
  }
  // Draw 8 scan direction arrows showing normal positive (outward) vs normal negative (inward)
  const bool outwardScan = config.scanDirection == EdgeLineScanDirection::NormalPositive;
  const double startOffset = outwardScan ? -static_cast<double>(config.innerBand) : static_cast<double>(config.outerBand);
  const double endOffset = outwardScan ? static_cast<double>(config.outerBand) : -static_cast<double>(config.innerBand);

  double startAngle = 0.0;
  double angleSpan = 2.0 * CV_PI;
  if (config.useArc)
  {
    startAngle = config.startAngleRadians;
    angleSpan = config.endAngleRadians - config.startAngleRadians;
    while (angleSpan < 0.0) angleSpan += 2.0 * CV_PI;
  }

  for (int i = 0; i < 8; ++i)
  {
    double angle = startAngle + angleSpan * static_cast<double>(i) / 7.0;
    const cv::Point2d radial(std::cos(angle), std::sin(angle));
    cv::Point2d ptStart = config.guideCenter + radial * (config.guideRadius + startOffset);
    cv::Point2d ptEnd = config.guideCenter + radial * (config.guideRadius + endOffset);

    // Draw arrow
    cv::Point startPt = surfacePoint(ptStart);
    cv::Point endPt = surfacePoint(ptEnd);
    cv::arrowedLine(result.diagnosticImage, startPt, endPt, cv::Scalar(16, 16, 16), 3, cv::LINE_AA, 0, 0.25);
    cv::arrowedLine(result.diagnosticImage, startPt, endPt, cv::Scalar(0, 255, 255), 1, cv::LINE_AA, 0, 0.25);
  }

  // Draw transition text label B->N (Light to Dark) vs N->B (Dark to Light) near center of circle/arc
  std::string transitionText = (config.transition == EdgeLineTransition::DarkToLight) ? "N->B" : "B->N";
  std::string dirText = outwardScan ? "OUT" : "IN";
  std::string fullLabel = transitionText + " (" + dirText + ")";

  cv::Point textPos = surfacePoint(config.guideCenter + cv::Point2d(-35, 5));
  cv::putText(result.diagnosticImage, fullLabel, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::putText(result.diagnosticImage, fullLabel, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

  // Draw raw edge points
  for (const cv::Point2d& point : result.rawEdgePoints)
  {
    cv::circle(result.diagnosticImage, surfacePoint(point), 1, cv::Scalar(0, 120, 255), -1, cv::LINE_AA);
  }

  // Draw filtered edge points with high visibility (fine dots with shadow)
  for (const cv::Point2d& point : result.edgePoints)
  {
    const cv::Point imagePoint = surfacePoint(point);
    // Draw black outline shadow first
    cv::circle(result.diagnosticImage, imagePoint, 3, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
    // Draw main colored dot (cyan/yellow)
    cv::circle(result.diagnosticImage, imagePoint, 2, cv::Scalar(0, 255, 255), -1, cv::LINE_AA);
  }

  const double span = configuredArcSpanRadians(config);
  const int requiredMinPoints = config.useArc
    ? std::max(8, static_cast<int>(std::round(static_cast<double>(config.minPoints) * span / (2.0 * CV_PI))))
    : config.minPoints;
  if (static_cast<int>(result.edgePoints.size()) < requiredMinPoints)
  {
    result.message = QString("Punti edge cerchio insufficienti: %1/%2").arg(result.edgePoints.size()).arg(requiredMinPoints);
    return result;
  }

  CircleFitSettings settings;
  settings.minPoints = config.minPoints;
  const CircleFitResult fit = CircleFit::fit(toIntegerPoints(result.edgePoints), settings);
  if (!fit.found)
  {
    result.message = "Fit cerchio edge fallito";
    return result;
  }

  result.found = true;
  result.circle.meta.id = config.id;
  result.circle.meta.label = config.label;
  result.circle.meta.method = "edge_circle";
  result.circle.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.circle.meta.valid = true;
  result.circle.meta.score = fit.inputPoints <= 0 ? 0.0 : static_cast<double>(fit.usedPoints) / static_cast<double>(fit.inputPoints);
  result.circle.center = fit.center;
  result.circle.radius = fit.radius;
  result.circle.meanError = fit.meanError;
  if (config.useArc)
  {
    result.circle.meta.method = "edge_arc_circle_fit";
    result.arc.meta = result.circle.meta;
    result.arc.meta.method = "edge_arc";
    result.arc.center = fit.center;
    result.arc.radius = fit.radius;
    result.arc.startAngleRadians = config.startAngleRadians;
    result.arc.endAngleRadians = config.endAngleRadians;
    result.arc.meanError = fit.meanError;
    // Draw shadow first
    cv::ellipse(result.diagnosticImage, surfacePoint(fit.center),
      cv::Size(static_cast<int>(std::round(fit.radius)), static_cast<int>(std::round(fit.radius))),
      0.0, startDegrees, endDegrees, cv::Scalar(16, 16, 16), 5, cv::LINE_AA);
    // Draw main line
    cv::ellipse(result.diagnosticImage, surfacePoint(fit.center),
      cv::Size(static_cast<int>(std::round(fit.radius)), static_cast<int>(std::round(fit.radius))),
      0.0, startDegrees, endDegrees, cv::Scalar(216, 79, 255), 2, cv::LINE_AA);
  }
  else
  {
    // Draw shadow first
    cv::circle(result.diagnosticImage, surfacePoint(fit.center), static_cast<int>(std::round(fit.radius)), cv::Scalar(16, 16, 16), 5, cv::LINE_AA);
    // Draw main line
    cv::circle(result.diagnosticImage, surfacePoint(fit.center), static_cast<int>(std::round(fit.radius)), cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
  }
  return result;
}
