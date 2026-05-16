#include "SurfaceDefectProcessor.h"

#include "processing/SurfaceEdgeStrategy.h"
#include "processing/SurfaceThresholdStrategy.h"
#include "processing/SurfaceTwoCirclesStrategy.h"

SurfaceDefectResult SurfaceDefectProcessor::detectByGrayscaleThreshold(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings) const
{
  return SurfaceThresholdStrategy().detectInRoi(input, searchRoi, exclusionRects, settings);
}

SurfaceStrategyResult SurfaceDefectProcessor::locateTwoCirclesAxis(
  const cv::Mat& input,
  const SurfaceTwoCirclesStrategyConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  return SurfaceTwoCirclesStrategy().locate(input, config, exclusionRects);
}

SurfaceDefectResult SurfaceDefectProcessor::locateAnnulusByGrayscaleThreshold(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  return SurfaceThresholdStrategy().locateAnnulus(input, config, exclusionRects);
}

SurfaceDefectResult SurfaceDefectProcessor::locateAnnulusByEdge(
  const cv::Mat& input,
  const SurfaceAnnulusThresholdConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  return SurfaceEdgeStrategy().locateAnnulus(input, config, exclusionRects);
}
