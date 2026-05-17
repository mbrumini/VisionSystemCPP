#include "ThresholdLineDetector.h"

#include "processing/SurfaceProcessingUtils.h"
#include "processing/geometry/ThresholdMaskBuilder.h"

#include <opencv2/imgproc.hpp>

namespace
{
std::vector<cv::Point> offsetContour(const std::vector<cv::Point>& contour, const cv::Rect& roi)
{
  std::vector<cv::Point> result;
  result.reserve(contour.size());

  for (const cv::Point& point : contour)
  {
    result.emplace_back(point.x + roi.x, point.y + roi.y);
  }

  return result;
}

LineGeometry lineFromFit(const std::vector<cv::Point>& points, const QString& id, const QString& label)
{
  cv::Vec4f fit;
  cv::fitLine(points, fit, cv::DIST_L2, 0.0, 0.01, 0.01);

  const cv::Point2d direction(fit[0], fit[1]);
  const cv::Point2d center(fit[2], fit[3]);
  double minProjection = 0.0;
  double maxProjection = 0.0;
  bool first = true;

  for (const cv::Point& point : points)
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
  line.meta.method = "threshold_line";
  line.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  line.meta.valid = true;
  line.meta.score = 1.0;
  line.start = center + direction * minProjection;
  line.end = center + direction * maxProjection;
  return line;
}
}

ThresholdLineDetectorResult ThresholdLineDetector::detect(
  const cv::Mat& input,
  const ThresholdLineDetectorConfig& config) const
{
  ThresholdLineDetectorResult result;

  if (input.empty() || config.roi.rect.isEmpty())
  {
    result.message = "Input o ROI linea non validi";
    return result;
  }

  const cv::Rect imageBounds(0, 0, input.cols, input.rows);
  const cv::Rect roi(
    config.roi.rect.x(),
    config.roi.rect.y(),
    config.roi.rect.width(),
    config.roi.rect.height());
  const cv::Rect clippedRoi = roi & imageBounds;

  if (clippedRoi.empty())
  {
    result.message = "ROI linea fuori immagine";
    return result;
  }

  cv::Mat mask = buildThresholdMask(input, clippedRoi, config.threshold, config.roi.exclusions);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  if (input.channels() == 1)
  {
    cv::cvtColor(input, result.diagnosticImage, cv::COLOR_GRAY2BGR);
  }
  else
  {
    input.copyTo(result.diagnosticImage);
  }

  cv::rectangle(result.diagnosticImage, clippedRoi, cv::Scalar(0, 255, 255), 2);
  result.processed = true;

  double bestArea = 0.0;
  std::vector<cv::Point> bestContour;
  for (const std::vector<cv::Point>& contour : contours)
  {
    const double area = cv::contourArea(contour);
    if (area < config.minArea || area <= bestArea)
    {
      continue;
    }

    bestArea = area;
    bestContour = contour;
  }

  if (bestContour.size() < 2)
  {
    result.message = "Linea non trovata";
    return result;
  }

  const std::vector<cv::Point> imageContour = offsetContour(bestContour, clippedRoi);
  result.line = lineFromFit(imageContour, config.id, config.label);
  result.found = result.line.meta.valid;

  cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{imageContour}, 0, cv::Scalar(0, 255, 0), 2);
  cv::line(
    result.diagnosticImage,
    surfacePoint(result.line.start),
    surfacePoint(result.line.end),
    cv::Scalar(255, 255, 0),
    2,
    cv::LINE_AA);
  return result;
}
