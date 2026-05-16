#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <vector>

struct SurfaceThresholdSettings
{
  int minValue = 0;
  int maxValue = 80;
};

struct SurfaceBlob
{
  double area = 0.0;
  cv::Point2d center;
  cv::Rect boundingRect;
  std::vector<cv::Point> contour;
};

struct SurfaceDefectResult
{
  bool processed = false;
  double totalArea = 0.0;
  std::vector<SurfaceBlob> blobs;
  cv::Mat diagnosticImage;
};

struct SurfaceCircleFeatureConfig
{
  std::string id;
  std::string polarity = "NB";
  cv::Rect searchRoi;
  SurfaceThresholdSettings threshold;
  double expectedRadiusMin = 0.0;
  double expectedRadiusMax = 0.0;
};

struct SurfaceTwoCirclesStrategyConfig
{
  std::vector<SurfaceCircleFeatureConfig> features;
  std::string originFeatureId = "midpoint";
  std::string xAxisFromFeatureId;
  std::string xAxisToFeatureId;
};

struct SurfaceCircleFeatureResult
{
  bool found = false;
  std::string id;
  std::string polarity;
  double area = 0.0;
  double radius = 0.0;
  cv::Point2d center;
  cv::Rect boundingRect;
  std::vector<cv::Point> contour;
};

struct SurfaceStrategyResult
{
  bool found = false;
  std::string strategyName;
  cv::Point2d origin;
  cv::Point2d xAxisStart;
  cv::Point2d xAxisEnd;
  cv::Point2d yAxisStart;
  cv::Point2d yAxisEnd;
  std::vector<SurfaceCircleFeatureResult> features;
  cv::Mat diagnosticImage;
};

struct SurfaceAnnulusThresholdConfig
{
  cv::Point center;
  int outerRadius = 0;
  int innerRadius = 0;
  SurfaceThresholdSettings threshold;
  int edgeSensitivity = 60;
};

class SurfaceDefectProcessor
{
public:
  SurfaceDefectResult detectByGrayscaleThreshold(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    const SurfaceThresholdSettings& settings = {}) const;

  SurfaceStrategyResult locateTwoCirclesAxis(
    const cv::Mat& input,
    const SurfaceTwoCirclesStrategyConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;

  SurfaceDefectResult locateAnnulusByGrayscaleThreshold(
    const cv::Mat& input,
    const SurfaceAnnulusThresholdConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;

  SurfaceDefectResult locateAnnulusByEdge(
    const cv::Mat& input,
    const SurfaceAnnulusThresholdConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;
};
