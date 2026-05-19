#pragma once

#include "config/RecipeManager.h"
#include "processing/SurfaceDefectProcessor.h"

#include <QRect>
#include <QVector>

#include <opencv2/core.hpp>

#include <vector>

namespace SurfaceLocalizationAdapters
{
std::vector<cv::Rect> toCvRects(const QVector<QRect>& rects);
SurfaceTwoCirclesStrategyConfig toProcessorStrategy(const SurfaceLocalizationStrategyConfig& recipeConfig);
}
