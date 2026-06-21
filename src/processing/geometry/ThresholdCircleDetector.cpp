#include "ThresholdCircleDetector.h"

#include "processing/CircleFit.h"
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
}

ThresholdCircleDetectorResult ThresholdCircleDetector::detect(
  const cv::Mat& input,
  const ThresholdCircleDetectorConfig& config) const
{
  ThresholdCircleDetectorResult result;

  if (input.empty() || config.roi.rect.isEmpty())
  {
    result.message = "Input o ROI geometria non validi";
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
    result.message = "ROI geometria fuori immagine";
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
    if (area < config.minArea)
    {
      continue;
    }
    if (config.maxArea > 0.0 && area > config.maxArea)
    {
      continue;
    }
    if (area <= bestArea)
    {
      continue;
    }

    bestArea = area;
    bestContour = contour;
  }

  if (bestContour.empty())
  {
    result.message = "Cerchio geometria non trovato";
    return result;
  }

  const std::vector<cv::Point> imageContour = offsetContour(bestContour, clippedRoi);
  CircleFitSettings fitSettings;
  fitSettings.minPoints = 8;
  const CircleFitResult fit = CircleFit::fit(imageContour, fitSettings);
  if (!fit.found)
  {
    result.message = "Fit cerchio geometria fallito";
    return result;
  }

  result.found = true;
  result.circle.meta.id = config.id;
  result.circle.meta.label = config.label;
  result.circle.meta.method = "threshold_circle";
  result.circle.meta.coordinateSpace = GeometryCoordinateSpace::Image;
  result.circle.meta.valid = true;
  result.circle.meta.score = fit.inputPoints <= 0 ? 0.0 : static_cast<double>(fit.usedPoints) / static_cast<double>(fit.inputPoints);
  result.circle.center = fit.center;
  result.circle.radius = fit.radius;
  result.circle.meanError = fit.meanError;

  cv::drawContours(result.diagnosticImage, std::vector<std::vector<cv::Point>>{imageContour}, 0, cv::Scalar(0, 255, 0), 2);
  cv::circle(result.diagnosticImage, surfacePoint(fit.center), static_cast<int>(fit.radius), cv::Scalar(255, 255, 0), 2);
  drawStyledCenterOfMass(result.diagnosticImage, fit.center);
  return result;
}
