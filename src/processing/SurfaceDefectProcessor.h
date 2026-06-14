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

struct SurfaceLocalizationReference
{
  bool found = false;
  std::string method;
  cv::Point2d center;
  double radius = 0.0;
  double angleRadians = 0.0;
  double score = 0.0;
  double meanError = 0.0;
  int inputPoints = 0;
  int usedPoints = 0;
  cv::Point2d xAxisStart;
  cv::Point2d xAxisEnd;
  cv::Point2d yAxisStart;
  cv::Point2d yAxisEnd;
};

struct SurfaceDefectResult
{
  bool processed = false;
  double totalArea = 0.0;
  std::vector<SurfaceBlob> blobs;
  SurfaceLocalizationReference localization;
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
  double edgeFitMaxError = 8.0;
};

struct SurfaceShapeMatchConfig
{
  cv::Rect searchRoi;
  std::vector<cv::Point> modelContour;
  int edgeSensitivity = 60;
  double maxShapeDistance = 0.25;
  double minContourArea = 50.0;
  double minAreaRatio = 0.20;
  double maxAreaRatio = 5.00;
};

struct SurfaceTemplateMatchConfig
{
  cv::Rect searchRoi;
  cv::Mat modelImage;
  int edgeSensitivity = 60;
  double minScore = 0.45;
  double angleStartDegrees = -180.0;
  double angleEndDegrees = 180.0;
  double angleStepDegrees = 5.0;
};

class SurfaceDefectProcessor
{
public:
  SurfaceDefectResult detectByGrayscaleThreshold(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    const SurfaceThresholdSettings& settings = {}) const;

  SurfaceDefectResult detectByGrayscaleThreshold(
    const cv::Mat& input,
    const std::vector<cv::Point>& searchPolygon,
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

  SurfaceDefectResult locateByEdgePca(
    const cv::Mat& input,
    const cv::Rect& searchRoi,
    const std::vector<cv::Rect>& exclusionRects = {},
    int edgeSensitivity = 60) const;

  SurfaceDefectResult locateByEdgePca(
    const cv::Mat& input,
    const std::vector<cv::Point>& searchPolygon,
    const std::vector<cv::Rect>& exclusionRects = {},
    int edgeSensitivity = 60) const;

  SurfaceDefectResult locateByShapeMatching(
    const cv::Mat& input,
    const SurfaceShapeMatchConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;

  SurfaceDefectResult locateByTemplateMatching(
    const cv::Mat& input,
    const SurfaceTemplateMatchConfig& config,
    const std::vector<cv::Rect>& exclusionRects = {}) const;
};
