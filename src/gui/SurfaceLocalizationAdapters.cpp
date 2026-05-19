#include "SurfaceLocalizationAdapters.h"

namespace SurfaceLocalizationAdapters
{
std::vector<cv::Rect> toCvRects(const QVector<QRect>& rects)
{
  std::vector<cv::Rect> result;
  result.reserve(static_cast<size_t>(rects.size()));

  for (const QRect& rect : rects)
  {
    result.emplace_back(rect.x(), rect.y(), rect.width(), rect.height());
  }

  return result;
}

SurfaceTwoCirclesStrategyConfig toProcessorStrategy(const SurfaceLocalizationStrategyConfig& recipeConfig)
{
  SurfaceTwoCirclesStrategyConfig result;
  result.originFeatureId = recipeConfig.origin.toStdString();
  result.xAxisFromFeatureId = recipeConfig.xAxisFrom.toStdString();
  result.xAxisToFeatureId = recipeConfig.xAxisTo.toStdString();

  for (const SurfaceStrategyFeatureConfig& recipeFeature : recipeConfig.features)
  {
    SurfaceCircleFeatureConfig feature;
    feature.id = recipeFeature.id.toStdString();
    feature.polarity = recipeFeature.polarity.toStdString();
    feature.searchRoi = cv::Rect(
      recipeFeature.searchRoi.x(),
      recipeFeature.searchRoi.y(),
      recipeFeature.searchRoi.width(),
      recipeFeature.searchRoi.height());
    feature.threshold.minValue = recipeFeature.thresholdMin;
    feature.threshold.maxValue = recipeFeature.thresholdMax;
    feature.expectedRadiusMin = recipeFeature.expectedRadiusMin;
    feature.expectedRadiusMax = recipeFeature.expectedRadiusMax;
    result.features.push_back(feature);
  }

  return result;
}
}
