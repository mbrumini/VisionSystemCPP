#include "SurfaceDefectProcessor.h"

#include "processing/SurfaceEdgeStrategy.h"
#include "processing/SurfaceModelStrategy.h"
#include "processing/SurfacePcaStrategy.h"
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

SurfaceDefectResult SurfaceDefectProcessor::detectByGrayscaleThreshold(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  const SurfaceThresholdSettings& settings) const
{
  return SurfaceThresholdStrategy().detectInPolygon(input, searchPolygon, exclusionRects, settings);
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

SurfaceDefectResult SurfaceDefectProcessor::locateByEdgePca(
  const cv::Mat& input,
  const cv::Rect& searchRoi,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity) const
{
  return SurfacePcaStrategy().locateByEdgePca(input, searchRoi, exclusionRects, edgeSensitivity);
}

SurfaceDefectResult SurfaceDefectProcessor::locateByEdgePca(
  const cv::Mat& input,
  const std::vector<cv::Point>& searchPolygon,
  const std::vector<cv::Rect>& exclusionRects,
  int edgeSensitivity) const
{
  return SurfacePcaStrategy().locateByEdgePca(input, searchPolygon, exclusionRects, edgeSensitivity);
}

SurfaceDefectResult SurfaceDefectProcessor::locateByShapeMatching(
  const cv::Mat& input,
  const SurfaceShapeMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  return SurfaceModelStrategy().locateByShapeMatching(input, config, exclusionRects);
}

SurfaceDefectResult SurfaceDefectProcessor::locateByTemplateMatching(
  const cv::Mat& input,
  const SurfaceTemplateMatchConfig& config,
  const std::vector<cv::Rect>& exclusionRects) const
{
  return SurfaceModelStrategy().locateByTemplateMatching(input, config, exclusionRects);
}
