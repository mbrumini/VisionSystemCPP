#include "processing/thread/ThreadPhaseAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
struct ExternalCrestPoint
{
  double alongPx = 0.0;
  double radialPx = 0.0;
  int side = 0;
};

double median(std::vector<double> values)
{
  if (values.empty())
  {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

bool projectionFrame(
  const ThreadProfileResult& result,
  cv::Point2d& origin,
  cv::Point2d& alongAxis,
  cv::Point2d& radialAxis)
{
  if (result.warpedWidth < 2 || result.warpedHeight < 2)
  {
    return false;
  }

  const cv::Point2d topLeft(result.imageQuad[0].x, result.imageQuad[0].y);
  const cv::Point2d topRight(result.imageQuad[1].x, result.imageQuad[1].y);
  const cv::Point2d bottomLeft(result.imageQuad[3].x, result.imageQuad[3].y);
  const cv::Point2d center =
    (topLeft + topRight + cv::Point2d(result.imageQuad[2].x, result.imageQuad[2].y) + bottomLeft) * 0.25;
  const cv::Point2d along = topRight - topLeft;
  const double alongLength = std::hypot(along.x, along.y);
  if (alongLength <= 1e-6)
  {
    return false;
  }

  origin = center;
  alongAxis = along * (1.0 / alongLength);
  radialAxis = cv::Point2d(-alongAxis.y, alongAxis.x);
  return true;
}

double acuteAngleToAxisDegrees(const ExternalCrestPoint& first, const ExternalCrestPoint& second)
{
  const double deltaAlong = std::abs(second.alongPx - first.alongPx);
  const double deltaRadial = std::abs(second.radialPx - first.radialPx);
  if (deltaAlong <= 1e-6 || deltaRadial <= 1e-6)
  {
    return std::numeric_limits<double>::quiet_NaN();
  }

  return std::atan2(deltaRadial, deltaAlong) * 180.0 / CV_PI;
}

ExternalCrestPoint projectedPoint(
  const cv::Point2d& point,
  int side,
  const cv::Point2d& origin,
  const cv::Point2d& alongAxis,
  const cv::Point2d& radialAxis)
{
  const cv::Point2d delta = point - origin;
  return {
    delta.dot(alongAxis),
    delta.dot(radialAxis),
    side
  };
}
}

ThreadPhaseAnalysis ThreadPhaseAnalyzer::analyze(
  const ThreadProfileResult& result,
  const std::vector<ThreadCrestPoint>& crests) const
{
  ThreadPhaseAnalysis analysis;
  if (crests.size() < 2)
  {
    analysis.diagnostic = QStringLiteral("insufficient_crests");
    return analysis;
  }

  cv::Point2d origin;
  cv::Point2d alongAxis;
  cv::Point2d radialAxis;
  if (!projectionFrame(result, origin, alongAxis, radialAxis))
  {
    analysis.diagnostic = QStringLiteral("invalid_axis");
    return analysis;
  }

  std::vector<ExternalCrestPoint> points;
  points.reserve(crests.size() * 2);
  for (const ThreadCrestPoint& crest : crests)
  {
    points.push_back(projectedPoint(
      cv::Point2d(crest.imageTopX, crest.imageTopY),
      -1,
      origin,
      alongAxis,
      radialAxis));
    points.push_back(projectedPoint(
      cv::Point2d(crest.imageBottomX, crest.imageBottomY),
      1,
      origin,
      alongAxis,
      radialAxis));
  }

  std::sort(points.begin(), points.end(), [](const ExternalCrestPoint& left, const ExternalCrestPoint& right) {
    return left.alongPx < right.alongPx;
  });

  std::vector<double> phaseDifferences;
  phaseDifferences.reserve(points.size());
  for (size_t i = 1; i + 1 < points.size(); ++i)
  {
    const ExternalCrestPoint& left = points[i - 1];
    const ExternalCrestPoint& center = points[i];
    const ExternalCrestPoint& right = points[i + 1];
    if (left.side == center.side || center.side == right.side)
    {
      continue;
    }

    const double angleLeft = acuteAngleToAxisDegrees(left, center);
    const double angleRight = acuteAngleToAxisDegrees(center, right);
    if (!std::isfinite(angleLeft) || !std::isfinite(angleRight))
    {
      continue;
    }

    const double phaseDifference = std::abs(angleLeft - angleRight);
    phaseDifferences.push_back(phaseDifference);
    analysis.angleSamples.push_back({
      center.alongPx,
      phaseDifference
    });
  }

  if (phaseDifferences.empty())
  {
    analysis.diagnostic = QStringLiteral("insufficient_phase_angles");
    return analysis;
  }

  analysis.sampleCount = static_cast<int>(phaseDifferences.size());
  analysis.phaseDegrees = median(std::move(phaseDifferences));
  analysis.valid = std::isfinite(analysis.phaseDegrees);
  if (!analysis.valid)
  {
    analysis.diagnostic = QStringLiteral("invalid_phase");
  }
  return analysis;
}
