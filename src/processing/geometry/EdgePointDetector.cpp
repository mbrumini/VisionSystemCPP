#include "EdgePointDetector.h"

#include "processing/SurfaceProcessingUtils.h"
#include "processing/geometry/EdgeProfile.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
void drawCyanCross(cv::Mat& image, const cv::Point2d& point)
{
  const cv::Point center = surfacePoint(point);
  const int size = 8;
  const cv::Scalar orange(0, 140, 255);
  cv::line(
    image,
    cv::Point(center.x - size, center.y - size),
    cv::Point(center.x + size, center.y + size),
    orange,
    4,
    cv::LINE_AA);
  cv::line(
    image,
    cv::Point(center.x - size, center.y + size),
    cv::Point(center.x + size, center.y - size),
    orange,
    4,
    cv::LINE_AA);
}
}

EdgePointDetectorResult EdgePointDetector::detect(const cv::Mat& input, const EdgePointDetectorConfig& config) const
{
  EdgePointDetectorResult result;
  if (input.empty())
  {
    result.message = "Input punto edge non valido";
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

  const cv::Point2d axis = config.scanEnd - config.scanStart;
  const double length = std::hypot(axis.x, axis.y);
  if (length <= 0.000001)
  {
    result.message = "Scansione punto edge non valida";
    return result;
  }

  const cv::Point2d unit = axis * (1.0 / length);
  const int samples = std::max(2, static_cast<int>(std::round(length)));
  const int sensitivity = std::clamp(config.edgeSensitivity, 1, 255);
  const int gradientThreshold = std::max(2, (256 - sensitivity) / 12 + 1);
  if (!EdgeProfileSampler::pointInsideImage(blurred, config.scanStart))
  {
    result.message = "Scansione punto edge fuori immagine";
    return result;
  }

  const std::vector<EdgeProfileSample> profile =
    EdgeProfileSampler::sampleLine(blurred, config.scanStart, config.scanEnd, samples);
  for (int i = 1; i < static_cast<int>(profile.size()); ++i)
  {
    const double delta = profile[i].value - profile[i - 1].value;
    const bool matchesTransition =
      (config.transition == EdgeLineTransition::DarkToLight && delta >= gradientThreshold) ||
      (config.transition == EdgeLineTransition::LightToDark && delta <= -gradientThreshold);
    if (matchesTransition)
    {
      result.candidates.push_back(SubpixelEdgeLocator::locate(
        {profile[i - 1], profile[i]},
        config.transition,
        EdgeLinePickMode::First,
        gradientThreshold,
        config.useSubpixel).point);
    }
  }
  const EdgeProfileCandidate located = SubpixelEdgeLocator::locate(
    profile,
    config.transition,
    config.pickMode,
    gradientThreshold,
    config.useSubpixel);

  cv::arrowedLine(
    result.diagnosticImage,
    surfacePoint(config.scanStart),
    surfacePoint(config.scanEnd),
    cv::Scalar(255, 0, 255),
    2,
    cv::LINE_AA,
    0,
    0.08);
  for (const cv::Point2d& candidate : result.candidates)
  {
    cv::circle(result.diagnosticImage, surfacePoint(candidate), 3, cv::Scalar(0, 120, 255), cv::FILLED, cv::LINE_AA);
  }

  result.processed = true;
  if (!located.found)
  {
    result.message = "Punto edge non trovato";
    return result;
  }

  result.point.meta.id = config.id;
  result.point.meta.label = config.label;
  result.point.meta.method = "edge_point";
  result.point.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.point.meta.valid = true;
  result.point.meta.score = std::max(1.0, located.strength);
  result.point.point = located.point;
  result.found = true;
  drawCyanCross(result.diagnosticImage, located.point);
  return result;
}
