#include "SurfaceTwoCirclesStrategy.h"

#include "processing/SurfaceProcessingUtils.h"
#include "processing/SurfaceThresholdStrategy.h"

namespace
{
const SurfaceCircleFeatureResult* findFeature(
  const std::vector<SurfaceCircleFeatureResult>& features,
  const std::string& id)
{
  for (const SurfaceCircleFeatureResult& feature : features)
  {
    if (feature.id == id)
    {
      return &feature;
    }
  }

  return nullptr;
}
}

SurfaceStrategyResult SurfaceTwoCirclesStrategy::locate(
  const cv::Mat& input,
  const SurfaceTwoCirclesStrategyConfig& config,
  const std::vector<cv::Rect>& exclusionRects,
  bool createDiagnosticImage,
  bool drawContours) const
{
  SurfaceStrategyResult result;
  result.strategyName = "two_circles_axis";

  if (input.empty() || config.features.size() < 2)
  {
    result.processed = true;
    return result;
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

  SurfaceThresholdStrategy thresholdStrategy;

  for (const SurfaceCircleFeatureConfig& featureConfig : config.features)
  {
    const SurfaceDefectResult featureMask = thresholdStrategy.detectInRoi(
      input,
      featureConfig.searchRoi,
      exclusionRects,
      featureConfig.threshold,
      createDiagnosticImage,
      drawContours);

    SurfaceCircleFeatureResult feature;
    feature.id = featureConfig.id;
    feature.polarity = featureConfig.polarity;

    if (createDiagnosticImage)
    {
      cv::rectangle(result.diagnosticImage, featureConfig.searchRoi, cv::Scalar(0, 255, 255), 2);
    }

    if (!featureMask.blobs.empty())
    {
      const SurfaceBlob& blob = featureMask.blobs.front();
      feature.found = true;
      feature.area = blob.area;
      feature.radius = std::sqrt(blob.area / CV_PI);
      feature.center = blob.center;
      feature.boundingRect = blob.boundingRect;
      feature.contour = blob.contour;

      if (featureConfig.expectedRadiusMin > 0.0 && feature.radius < featureConfig.expectedRadiusMin)
      {
        feature.found = false;
      }

      if (featureConfig.expectedRadiusMax > 0.0 && feature.radius > featureConfig.expectedRadiusMax)
      {
        feature.found = false;
      }
    }

    if (feature.found)
    {
      if (createDiagnosticImage)
      {
        if (drawContours)
        {
          drawStyledContour(result.diagnosticImage, feature.contour, cv::Scalar(94, 197, 34));
        }
        // cv::rectangle(result.diagnosticImage, feature.boundingRect, cv::Scalar(255, 0, 0), 2);
        drawStyledCenterOfMass(result.diagnosticImage, feature.center);
        const cv::Point textPos = surfacePoint(feature.center + cv::Point2d(10, -10));
        cv::putText(result.diagnosticImage, feature.id, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(16, 16, 16), 4, cv::LINE_AA);
        cv::putText(result.diagnosticImage, feature.id, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
      }
    }
    else
    {
      if (createDiagnosticImage)
      {
        cv::putText(result.diagnosticImage, feature.id + "?", featureConfig.searchRoi.tl(), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
      }
    }

    result.features.push_back(feature);
  }

  const std::string fromId = config.xAxisFromFeatureId.empty() ? config.features[0].id : config.xAxisFromFeatureId;
  const std::string toId = config.xAxisToFeatureId.empty() ? config.features[1].id : config.xAxisToFeatureId;
  const SurfaceCircleFeatureResult* fromFeature = findFeature(result.features, fromId);
  const SurfaceCircleFeatureResult* toFeature = findFeature(result.features, toId);

  if (!fromFeature || !toFeature || !fromFeature->found || !toFeature->found)
  {
    result.processed = true;
    return result;
  }

  const cv::Point2d axisVector = toFeature->center - fromFeature->center;
  const double length = std::sqrt(axisVector.x * axisVector.x + axisVector.y * axisVector.y);

  if (length <= 0.0)
  {
    result.processed = true;
    return result;
  }

  const cv::Point2d xDirection(axisVector.x / length, axisVector.y / length);
  const cv::Point2d yDirection(-xDirection.y, xDirection.x);

  const SurfaceCircleFeatureResult* originFeature = findFeature(result.features, config.originFeatureId);
  result.origin = originFeature && originFeature->found
    ? originFeature->center
    : (fromFeature->center + toFeature->center) * 0.5;

  const double axisLength = std::max(60.0, length * 0.25);
  result.xAxisStart = result.origin - xDirection * axisLength;
  result.xAxisEnd = result.origin + xDirection * axisLength;
  result.yAxisStart = result.origin - yDirection * axisLength;
  result.yAxisEnd = result.origin + yDirection * axisLength;
  result.found = true;

  if (createDiagnosticImage)
  {
    drawStyledAxes(result.diagnosticImage, result.origin, result.xAxisStart, result.xAxisEnd, result.yAxisStart, result.yAxisEnd, cv::Scalar(0, 0, 255), cv::Scalar(255, 0, 255));
    drawStyledCenterOfMass(result.diagnosticImage, result.origin);
  }

  result.processed = true;
  return result;
}
