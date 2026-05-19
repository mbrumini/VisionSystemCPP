#include "EdgePointDetector.h"

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

void drawCyanCross(cv::Mat& image, const cv::Point2d& point)
{
  const cv::Point center = surfacePoint(point);
  const int size = 8;
  const cv::Scalar cyan(255, 255, 0);
  cv::line(
    image,
    cv::Point(center.x - size, center.y - size),
    cv::Point(center.x + size, center.y + size),
    cyan,
    2,
    cv::LINE_AA);
  cv::line(
    image,
    cv::Point(center.x - size, center.y + size),
    cv::Point(center.x + size, center.y - size),
    cyan,
    2,
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
  int bestStrength = -1;
  cv::Point2d bestPoint;
  bool found = false;

  cv::Point2d previousPoint = config.scanStart;
  if (!pointInsideImage(blurred, previousPoint))
  {
    result.message = "Scansione punto edge fuori immagine";
    return result;
  }

  int previousValue = sampledGray(blurred, previousPoint);
  for (int i = 1; i <= samples; ++i)
  {
    const cv::Point2d currentPoint = config.scanStart + unit * (length * static_cast<double>(i) / samples);
    if (!pointInsideImage(blurred, currentPoint))
    {
      previousPoint = currentPoint;
      continue;
    }

    const int currentValue = sampledGray(blurred, currentPoint);
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
      result.candidates.push_back(candidate);
      const int strength = std::abs(delta);
      if (config.pickMode == EdgeLinePickMode::First)
      {
        bestPoint = candidate;
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
  if (!found)
  {
    result.message = "Punto edge non trovato";
    return result;
  }

  result.point.meta.id = config.id;
  result.point.meta.label = config.label;
  result.point.meta.method = "edge_point";
  result.point.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.point.meta.valid = true;
  result.point.meta.score = static_cast<double>(std::max(1, bestStrength));
  result.point.point = bestPoint;
  result.found = true;
  drawCyanCross(result.diagnosticImage, bestPoint);
  return result;
}
