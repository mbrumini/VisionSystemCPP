#include "processing/geometry/EdgeCircleDetectorExperimental.h"

#include "processing/CircleFit.h"
#include "processing/SurfaceProcessingUtils.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <numeric>
#include <string>

namespace
{
constexpr double kEpsilon = 0.000001;

bool pointInsideImage(const cv::Mat& image, const cv::Point2d& point)
{
  return point.x >= 0.0 && point.y >= 0.0 && point.x < image.cols && point.y < image.rows;
}

double sampledGrayBilinear(const cv::Mat& gray, const cv::Point2d& point)
{
  const double x = std::clamp(point.x, 0.0, static_cast<double>(gray.cols - 1));
  const double y = std::clamp(point.y, 0.0, static_cast<double>(gray.rows - 1));
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, gray.cols - 1);
  const int y1 = std::min(y0 + 1, gray.rows - 1);
  const double tx = x - static_cast<double>(x0);
  const double ty = y - static_cast<double>(y0);
  const double top = gray.at<uchar>(y0, x0) * (1.0 - tx) + gray.at<uchar>(y0, x1) * tx;
  const double bottom = gray.at<uchar>(y1, x0) * (1.0 - tx) + gray.at<uchar>(y1, x1) * tx;
  return top * (1.0 - ty) + bottom * ty;
}

double median(std::vector<double> values)
{
  if (values.empty())
  {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  const size_t middle = values.size() / 2;
  if ((values.size() % 2) == 1)
  {
    return values[middle];
  }
  return (values[middle - 1] + values[middle]) * 0.5;
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

struct RadialCandidate
{
  cv::Point2d point;
  double strength = 0.0;
  double radius = 0.0;
  int sector = 0;
};

std::vector<RadialCandidate> scanCircleEdgeCandidates(const cv::Mat& gray,
                                                      const EdgeCircleDetectorConfig& config,
                                                      int gradientThreshold)
{
  std::vector<RadialCandidate> candidates;
  if (config.guideRadius <= 1.0 || config.innerBand <= 0 || config.outerBand <= 0 ||
      config.guideRadius <= static_cast<double>(config.innerBand))
  {
    return candidates;
  }

  double startAngle = 0.0;
  const double span = configuredArcSpanRadians(config);
  if (config.useArc)
  {
    startAngle = config.startAngleRadians;
  }

  const int fullCircleSamples = std::max(90, static_cast<int>(std::round(config.guideRadius * 2.0)));
  const int angleSamples = config.useArc
    ? std::max(12, static_cast<int>(std::round(fullCircleSamples * span / (2.0 * CV_PI))))
    : fullCircleSamples;
  const int sectors = std::clamp(angleSamples / 8, 12, 48);
  const bool outwardScan = config.scanDirection == EdgeLineScanDirection::NormalPositive;
  const double startOffset = outwardScan ? -static_cast<double>(config.innerBand) : static_cast<double>(config.outerBand);
  const double endOffset = outwardScan ? static_cast<double>(config.outerBand) : -static_cast<double>(config.innerBand);
  const int steps = std::max(4, (config.innerBand + config.outerBand) * 2);

  for (int i = 0; i < angleSamples; ++i)
  {
    const double angle = startAngle + span * static_cast<double>(i) / static_cast<double>(std::max(1, angleSamples - 1));
    const cv::Point2d radial(std::cos(angle), std::sin(angle));
    cv::Point2d previousPoint = config.guideCenter + radial * (config.guideRadius + startOffset);
    if (!pointInsideImage(gray, previousPoint))
    {
      continue;
    }

    double previousValue = sampledGrayBilinear(gray, previousPoint);
    bool found = false;
    RadialCandidate best;
    for (int step = 1; step <= steps; ++step)
    {
      const double offset = startOffset + (endOffset - startOffset) * static_cast<double>(step) / static_cast<double>(steps);
      const cv::Point2d currentPoint = config.guideCenter + radial * (config.guideRadius + offset);
      if (!pointInsideImage(gray, currentPoint))
      {
        previousPoint = currentPoint;
        continue;
      }

      const double currentValue = sampledGrayBilinear(gray, currentPoint);
      const double delta = currentValue - previousValue;
      const bool matchesTransition =
        (config.transition == EdgeLineTransition::DarkToLight && delta >= gradientThreshold) ||
        (config.transition == EdgeLineTransition::LightToDark && delta <= -gradientThreshold);
      if (matchesTransition)
      {
        const double target = (previousValue + currentValue) * 0.5;
        const double t = std::abs(delta) > kEpsilon
          ? std::clamp((target - previousValue) / delta, 0.0, 1.0)
          : 1.0;
        const cv::Point2d candidatePoint = previousPoint + (currentPoint - previousPoint) * t;
        const double strength = std::abs(delta);
        RadialCandidate candidate;
        candidate.point = candidatePoint;
        candidate.strength = strength;
        candidate.radius = cv::norm(candidatePoint - config.guideCenter);
        candidate.sector = std::clamp(
          static_cast<int>(std::floor(static_cast<double>(i) * sectors / std::max(1, angleSamples))),
          0,
          sectors - 1);

        if (config.pickMode == EdgeLinePickMode::First)
        {
          candidates.push_back(candidate);
          found = true;
          break;
        }

        if (!found ||
            config.pickMode == EdgeLinePickMode::Last ||
            candidate.strength > best.strength)
        {
          best = candidate;
          found = true;
        }
      }

      previousPoint = currentPoint;
      previousValue = currentValue;
    }

    if (found && config.pickMode != EdgeLinePickMode::First)
    {
      candidates.push_back(best);
    }
  }

  return candidates;
}

std::vector<cv::Point2d> keepBestPerSector(const std::vector<RadialCandidate>& candidates)
{
  if (candidates.empty())
  {
    return {};
  }

  std::vector<RadialCandidate> sorted = candidates;
  std::sort(sorted.begin(), sorted.end(), [](const RadialCandidate& a, const RadialCandidate& b) {
    if (a.sector == b.sector)
    {
      return a.strength > b.strength;
    }
    return a.sector < b.sector;
  });

  std::vector<cv::Point2d> points;
  int lastSector = -1;
  int keptInSector = 0;
  constexpr int kMaxPointsPerSector = 4;
  for (const RadialCandidate& candidate : sorted)
  {
    if (candidate.sector != lastSector)
    {
      lastSector = candidate.sector;
      keptInSector = 0;
    }

    if (keptInSector >= kMaxPointsPerSector)
    {
      continue;
    }

    points.push_back(candidate.point);
    ++keptInSector;
  }
  return points;
}

std::vector<cv::Point2d> filterByGuideMedianRadius(const std::vector<cv::Point2d>& points,
                                                   const EdgeCircleDetectorConfig& config)
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
  const double centerRadius = median(radii);
  const double maxDeviation = static_cast<double>(config.edgeStatisticalFilter);

  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    if (std::abs(cv::norm(point - config.guideCenter) - centerRadius) <= maxDeviation)
    {
      filtered.push_back(point);
    }
  }
  return filtered;
}

CircleFitResult fitCircleFromDoublePoints(const std::vector<cv::Point2d>& points, int minPoints)
{
  if (static_cast<int>(points.size()) < minPoints)
  {
    return {};
  }

  std::vector<cv::Point> integerPoints;
  integerPoints.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    integerPoints.push_back(surfacePoint(point));
  }

  CircleFitSettings settings;
  settings.minPoints = minPoints;
  return CircleFit::fit(integerPoints, settings);
}

std::vector<cv::Point2d> filterByFitResidual(const std::vector<cv::Point2d>& points,
                                             const CircleFitResult& fit,
                                             double maxResidual)
{
  if (!fit.found || points.size() < 3 || maxResidual <= 0.0)
  {
    return points;
  }

  std::vector<double> residuals;
  residuals.reserve(points.size());
  for (const cv::Point2d& point : points)
  {
    residuals.push_back(std::abs(cv::norm(point - fit.center) - fit.radius));
  }
  const double residualCenter = median(residuals);
  const double limit = std::max(maxResidual, residualCenter * 2.0);

  std::vector<cv::Point2d> filtered;
  filtered.reserve(points.size());
  for (size_t i = 0; i < points.size(); ++i)
  {
    if (residuals[i] <= limit)
    {
      filtered.push_back(points[i]);
    }
  }
  return filtered;
}

void drawDiagnostics(cv::Mat& image,
                     const EdgeCircleDetectorConfig& config,
                     const std::vector<cv::Point2d>& rawPoints,
                     const std::vector<cv::Point2d>& filteredPoints,
                     const CircleFitResult& fit)
{
  const double startDegrees = config.startAngleRadians * 180.0 / CV_PI;
  double endDegrees = config.endAngleRadians * 180.0 / CV_PI;
  if (config.useArc && endDegrees < startDegrees)
  {
    endDegrees += 360.0;
  }

  if (config.useArc)
  {
    cv::ellipse(image, surfacePoint(config.guideCenter),
      cv::Size(static_cast<int>(std::round(config.guideRadius)), static_cast<int>(std::round(config.guideRadius))),
      0.0, startDegrees, endDegrees, cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
  }
  else
  {
    cv::circle(image, surfacePoint(config.guideCenter), static_cast<int>(std::round(config.guideRadius)), cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
  }

  for (const cv::Point2d& point : rawPoints)
  {
    cv::circle(image, surfacePoint(point), 1, cv::Scalar(0, 120, 255), -1, cv::LINE_AA);
  }
  for (const cv::Point2d& point : filteredPoints)
  {
    const cv::Point imagePoint = surfacePoint(point);
    cv::circle(image, imagePoint, 3, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
    cv::circle(image, imagePoint, 2, cv::Scalar(0, 255, 255), -1, cv::LINE_AA);
  }

  const cv::Point legendAnchor = surfacePoint(config.guideCenter + cv::Point2d(-config.guideRadius, -config.guideRadius));
  const cv::Point textPoint(legendAnchor.x + 12, legendAnchor.y - 28);
  const std::string label = QString("ROBUST  raw=%1  active=%2")
    .arg(rawPoints.size())
    .arg(filteredPoints.size())
    .toStdString();
  cv::putText(image, label, textPoint, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::putText(image, label, textPoint, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

  const cv::Point rawPoint(textPoint.x, textPoint.y + 22);
  const cv::Point activePoint(textPoint.x + 88, textPoint.y + 22);
  cv::circle(image, rawPoint, 3, cv::Scalar(0, 120, 255), -1, cv::LINE_AA);
  cv::putText(image, "raw", rawPoint + cv::Point(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::putText(image, "raw", rawPoint + cv::Point(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 120, 255), 1, cv::LINE_AA);
  cv::circle(image, activePoint, 5, cv::Scalar(16, 16, 16), -1, cv::LINE_AA);
  cv::circle(image, activePoint, 3, cv::Scalar(0, 255, 255), -1, cv::LINE_AA);
  cv::putText(image, "active", activePoint + cv::Point(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(16, 16, 16), 3, cv::LINE_AA);
  cv::putText(image, "active", activePoint + cv::Point(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

  if (fit.found)
  {
    if (config.useArc)
    {
      cv::ellipse(image, surfacePoint(fit.center),
        cv::Size(static_cast<int>(std::round(fit.radius)), static_cast<int>(std::round(fit.radius))),
        0.0, startDegrees, endDegrees, cv::Scalar(216, 79, 255), 2, cv::LINE_AA);
    }
    else
    {
      cv::circle(image, surfacePoint(fit.center), static_cast<int>(std::round(fit.radius)), cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
    }
  }
}
}

bool robustEdgeCircleDetectorEnabled()
{
  const char* forceStandard = std::getenv("VISION_EDGE_CIRCLE_STANDARD");
  if (forceStandard)
  {
    std::string normalized(forceStandard);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
    {
      return false;
    }
  }

  const char* value = std::getenv("VISION_EDGE_CIRCLE_EXPERIMENTAL");
  if (!value)
  {
    value = std::getenv("VISION_EXPERIMENTAL_EDGE_CIRCLE");
  }
  if (!value)
  {
    return true;
  }

  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

EdgeCircleDetectorResult detectEdgeCircleWithSelectedDetector(const cv::Mat& input,
                                                              const EdgeCircleDetectorConfig& config)
{
  if (robustEdgeCircleDetectorEnabled())
  {
    return EdgeCircleDetectorExperimental().detect(input, config);
  }

  return EdgeCircleDetector().detect(input, config);
}

EdgeCircleDetectorResult EdgeCircleDetectorExperimental::detect(const cv::Mat& input,
                                                                const EdgeCircleDetectorConfig& config) const
{
  EdgeCircleDetectorResult result;
  if (input.empty())
  {
    result.message = "Input cerchio edge sperimentale non valido";
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

  if (config.guideRadius <= static_cast<double>(config.innerBand))
  {
    result.processed = true;
    result.message = "Banda interna cerchio edge sperimentale non valida";
    return result;
  }

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0.0);
  const int sensitivity = std::clamp(config.edgeSensitivity, 1, 255);
  const int gradientThreshold = std::max(2, (256 - sensitivity) / 12 + 1);

  const std::vector<RadialCandidate> candidates =
    scanCircleEdgeCandidates(blurred, config, gradientThreshold);
  result.rawEdgePoints.reserve(candidates.size());
  for (const RadialCandidate& candidate : candidates)
  {
    result.rawEdgePoints.push_back(candidate.point);
  }

  std::vector<cv::Point2d> edgePoints = keepBestPerSector(candidates);
  edgePoints = filterByGuideMedianRadius(edgePoints, config);
  const double span = configuredArcSpanRadians(config);
  const int requiredMinPoints = config.useArc
    ? std::max(8, static_cast<int>(std::round(static_cast<double>(config.minPoints) * span / (2.0 * CV_PI))))
    : config.minPoints;

  result.processed = true;
  if (static_cast<int>(edgePoints.size()) < requiredMinPoints)
  {
    result.edgePoints = edgePoints;
    drawDiagnostics(result.diagnosticImage, config, result.rawEdgePoints, result.edgePoints, {});
    result.message = QString("Punti edge cerchio sperimentale insufficienti: %1/%2").arg(edgePoints.size()).arg(requiredMinPoints);
    return result;
  }

  CircleFitResult initialFit = fitCircleFromDoublePoints(edgePoints, requiredMinPoints);
  if (!initialFit.found)
  {
    result.edgePoints = edgePoints;
    drawDiagnostics(result.diagnosticImage, config, result.rawEdgePoints, result.edgePoints, {});
    result.message = "Fit cerchio edge sperimentale fallito";
    return result;
  }

  const double residualLimit = std::max(2.0, static_cast<double>(config.edgeCleanupDerivative));
  result.edgePoints = filterByFitResidual(edgePoints, initialFit, residualLimit);
  CircleFitResult fit = fitCircleFromDoublePoints(result.edgePoints, requiredMinPoints);
  if (!fit.found)
  {
    fit = initialFit;
    result.edgePoints = edgePoints;
  }

  drawDiagnostics(result.diagnosticImage, config, result.rawEdgePoints, result.edgePoints, fit);

  result.found = fit.found;
  result.circle.meta.id = config.id;
  result.circle.meta.label = config.label;
  result.circle.meta.method = config.useArc ? "edge_arc_robust" : "edge_circle_robust";
  result.circle.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.circle.meta.valid = fit.found;
  result.circle.meta.score = fit.inputPoints <= 0 ? 0.0 : static_cast<double>(fit.usedPoints) / static_cast<double>(fit.inputPoints);
  result.circle.center = fit.center;
  result.circle.radius = fit.radius;
  result.circle.meanError = fit.meanError;

  if (config.useArc && fit.found)
  {
    result.arc.meta = result.circle.meta;
    result.arc.meta.method = "edge_arc_robust";
    result.arc.center = fit.center;
    result.arc.radius = fit.radius;
    result.arc.startAngleRadians = config.startAngleRadians;
    result.arc.endAngleRadians = config.endAngleRadians;
    result.arc.meanError = fit.meanError;
  }

  if (!fit.found)
  {
    result.message = "Fit cerchio edge sperimentale fallito";
  }
  return result;
}
