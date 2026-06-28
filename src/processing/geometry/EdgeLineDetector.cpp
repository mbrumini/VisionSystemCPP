#include "EdgeLineDetector.h"

#include "processing/SurfaceProcessingUtils.h"
#include "processing/geometry/EdgePointFilters.h"
#include "processing/geometry/EdgeProfile.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kEpsilon = 0.000001;

cv::Rect bandBoundingRect(const cv::Point2d& start, const cv::Point2d& end, double halfWidth, const cv::Size& imageSize)
{
  const cv::Point2d direction = end - start;
  const double length = std::hypot(direction.x, direction.y);
  const cv::Point2d unit = length <= 0.000001 ? cv::Point2d(1.0, 0.0) : direction * (1.0 / length);
  const cv::Point2d normal(-unit.y, unit.x);
  const cv::Point2d p1 = start + normal * halfWidth;
  const cv::Point2d p2 = end + normal * halfWidth;
  const cv::Point2d p3 = end - normal * halfWidth;
  const cv::Point2d p4 = start - normal * halfWidth;
  const double minX = std::min({p1.x, p2.x, p3.x, p4.x});
  const double maxX = std::max({p1.x, p2.x, p3.x, p4.x});
  const double minY = std::min({p1.y, p2.y, p3.y, p4.y});
  const double maxY = std::max({p1.y, p2.y, p3.y, p4.y});
  const cv::Rect roi(
    static_cast<int>(std::floor(minX)),
    static_cast<int>(std::floor(minY)),
    static_cast<int>(std::ceil(maxX - minX)),
    static_cast<int>(std::ceil(maxY - minY)));
  return roi & cv::Rect(0, 0, imageSize.width, imageSize.height);
}

bool pointInsideGuideBand(
  const cv::Point2d& point,
  const cv::Point2d& start,
  const cv::Point2d& end,
  double halfWidth)
{
  const cv::Point2d axis = end - start;
  const double length = std::hypot(axis.x, axis.y);
  if (length <= 0.000001)
  {
    return false;
  }

  const cv::Point2d unit = axis * (1.0 / length);
  const cv::Point2d delta = point - start;
  const double along = delta.x * unit.x + delta.y * unit.y;
  if (along < 0.0 || along > length)
  {
    return false;
  }

  const double cross = std::abs(delta.x * unit.y - delta.y * unit.x);
  return cross <= halfWidth;
}

std::vector<cv::Point2d> scanLineEdges(
  const cv::Mat& gray,
  const EdgeLineDetectorConfig& config,
  int gradientThreshold)
{
  std::vector<cv::Point2d> points;
  const cv::Point2d axis = config.guideEnd - config.guideStart;
  const double length = std::hypot(axis.x, axis.y);
  if (length <= 0.000001 || config.bandHalfWidth <= 0)
  {
    return points;
  }

  const cv::Point2d unit = axis * (1.0 / length);
  cv::Point2d normal(-unit.y, unit.x);
  if (config.scanDirection == EdgeLineScanDirection::NormalNegative)
  {
    normal *= -1.0;
  }

  const int alongSamples = std::max(2, static_cast<int>(std::round(length)));
  const int halfWidth = std::max(1, config.bandHalfWidth);
  for (int i = 0; i <= alongSamples; ++i)
  {
    const cv::Point2d base = config.guideStart + unit * (length * static_cast<double>(i) / alongSamples);
    const cv::Point2d scanStart = base - normal * halfWidth;
    const cv::Point2d scanEnd = base + normal * halfWidth;
    if (!EdgeProfileSampler::pointInsideImage(gray, scanStart))
    {
      continue;
    }

    const std::vector<EdgeProfileSample> profile = EdgeProfileSampler::sampleLine(gray, scanStart, scanEnd, halfWidth * 2);
    const EdgeProfileCandidate located = SubpixelEdgeLocator::locate(
      profile,
      config.transition,
      config.pickMode,
      gradientThreshold,
      config.useSubpixel);
    if (located.found)
    {
      points.push_back(located.point);
    }
  }

  return points;
}

void drawScanDiagnostics(cv::Mat& image, const EdgeLineDetectorConfig& config)
{
  const cv::Point2d axis = config.guideEnd - config.guideStart;
  const double length = std::hypot(axis.x, axis.y);
  if (length <= 0.000001 || config.bandHalfWidth <= 0)
  {
    return;
  }

  const cv::Point2d unit = axis * (1.0 / length);
  cv::Point2d normal(-unit.y, unit.x);
  if (config.scanDirection == EdgeLineScanDirection::NormalNegative)
  {
    normal *= -1.0;
  }

  const int guideCount = 12;
  for (int i = 0; i <= guideCount; ++i)
  {
    const cv::Point2d base = config.guideStart + unit * (length * static_cast<double>(i) / guideCount);
    const cv::Point2d scanStart = base - normal * config.bandHalfWidth;
    const cv::Point2d scanEnd = base + normal * config.bandHalfWidth;
    cv::line(image, surfacePoint(scanStart), surfacePoint(scanEnd), cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
  }

  const cv::Point2d arrowBase = config.guideStart + unit * (length * 0.5);
  cv::arrowedLine(
    image,
    surfacePoint(arrowBase - normal * config.bandHalfWidth),
    surfacePoint(arrowBase + normal * config.bandHalfWidth),
    cv::Scalar(255, 0, 255),
    2,
    cv::LINE_AA,
    0,
    0.25);
}

LineGeometry fitLineGeometry(
  const std::vector<cv::Point2d>& points,
  const QString& id,
  const QString& label,
  const cv::Point2d& guideStart,
  const cv::Point2d& guideEnd)
{
  cv::Vec4f fit;
  cv::fitLine(points, fit, cv::DIST_L2, 0.0, 0.01, 0.01);

  cv::Point2d direction(fit[0], fit[1]);
  const cv::Point2d guideAxis = guideEnd - guideStart;
  const double guideLength = std::hypot(guideAxis.x, guideAxis.y);
  if (guideLength > kEpsilon && direction.dot(guideAxis) < 0.0)
  {
    direction *= -1.0;
  }
  const cv::Point2d center(fit[2], fit[3]);
  double minProjection = 0.0;
  double maxProjection = 0.0;
  bool first = true;

  for (const cv::Point2d& point : points)
  {
    const cv::Point2d delta(point.x - center.x, point.y - center.y);
    const double projection = delta.x * direction.x + delta.y * direction.y;
    if (first)
    {
      minProjection = projection;
      maxProjection = projection;
      first = false;
      continue;
    }

    minProjection = std::min(minProjection, projection);
    maxProjection = std::max(maxProjection, projection);
  }

  LineGeometry line;
  line.meta.id = id;
  line.meta.label = label;
  line.meta.method = "edge_line";
  line.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  line.meta.valid = true;
  line.meta.score = 1.0;
  line.start = center + direction * minProjection;
  line.end = center + direction * maxProjection;
  line.edgePoints = points;
  return line;
}
}

EdgeLineDetectorResult EdgeLineDetector::detect(const cv::Mat& input, const EdgeLineDetectorConfig& config) const
{
  EdgeLineDetectorResult result;
  if (input.empty())
  {
    result.message = "Input linea edge non valido";
    return result;
  }

  result.searchRoi = bandBoundingRect(config.guideStart, config.guideEnd, config.bandHalfWidth, input.size());
  if (result.searchRoi.empty())
  {
    result.message = "Fascia linea fuori immagine";
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
  result.rawEdgePoints = scanLineEdges(blurred, config, gradientThreshold);
  result.derivativeEdgePoints = EdgePointFilters::filterByDerivative(
    result.rawEdgePoints,
    {config.guideStart, config.guideEnd, config.edgeCleanupDerivative});
  result.statisticalEdgePoints = EdgePointFilters::filterByNormalMedianDeviation(
    result.derivativeEdgePoints,
    {config.guideStart, config.guideEnd, config.edgeStatisticalFilter});
  result.edgePoints = result.statisticalEdgePoints;

  cv::rectangle(result.diagnosticImage, result.searchRoi, cv::Scalar(0, 255, 255), 2);
  drawScanDiagnostics(result.diagnosticImage, config);
  for (const cv::Point2d& point : result.rawEdgePoints)
  {
    cv::circle(result.diagnosticImage, surfacePoint(point), 3, cv::Scalar(0, 120, 255), cv::FILLED, cv::LINE_AA);
  }
  for (const cv::Point2d& point : result.edgePoints)
  {
    const cv::Point imagePoint = surfacePoint(point);
    cv::circle(result.diagnosticImage, imagePoint, 6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    cv::circle(result.diagnosticImage, imagePoint, 5, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);
  }
  result.processed = true;

  if (static_cast<int>(result.edgePoints.size()) < config.minPoints)
  {
    result.message = QString("Punti edge linea insufficienti: %1").arg(result.edgePoints.size());
    return result;
  }

  result.line = fitLineGeometry(result.edgePoints, config.id, config.label, config.guideStart, config.guideEnd);
  result.line.meta.score = static_cast<double>(result.edgePoints.size());
  result.found = true;

  cv::line(
    result.diagnosticImage,
    surfacePoint(result.line.start),
    surfacePoint(result.line.end),
    cv::Scalar(0, 255, 0),
    4,
    cv::LINE_AA);
  return result;
}
