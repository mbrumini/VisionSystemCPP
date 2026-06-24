#pragma once

#include "config/RecipeJsonUtils.h"
#include "processing/geometry/EdgeLineDetector.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <opencv2/core/types.hpp>

namespace GeometryRecipePersistence
{
QString scanDirectionToRecipe(EdgeLineScanDirection direction);
EdgeLineScanDirection scanDirectionFromRecipe(const QString& value);

QString transitionToRecipe(EdgeLineTransition transition);
EdgeLineTransition transitionFromRecipe(const QString& value);

QString pickModeToRecipe(EdgeLinePickMode mode);
EdgeLinePickMode pickModeFromRecipe(const QString& value);

template<typename Config>
QString nextGeometryId(const QString& prefix, const QVector<Config>& configs);

struct SegmentGuideRecipeCoords
{
  QString coordinateSpace;
  QPointF partStart;
  QPointF partEnd;
  QPointF imageStart;
  QPointF imageEnd;
};

struct CenterGuideRecipeCoords
{
  QString coordinateSpace;
  QPointF partCenter;
  QPointF imageCenter;
};

struct ArcGuideRecipeCoords
{
  QString coordinateSpace;
  QPointF partCenter;
  QPointF partStart;
  QPointF partEnd;
  QPointF partThrough;
  QPointF imageCenter;
  QPointF imageStart;
  QPointF imageEnd;
  QPointF imageThrough;
};

SegmentGuideRecipeCoords segmentGuideFromRuntime(bool hasPartGuide,
                                                 const cv::Point2d& partStart,
                                                 const cv::Point2d& partEnd,
                                                 bool hasImageGuide,
                                                 const cv::Point2d& imageStart,
                                                 const cv::Point2d& imageEnd);

void applySegmentGuideRuntime(bool* hasPartGuide,
                              bool* hasImageGuide,
                              cv::Point2d* partStart,
                              cv::Point2d* partEnd,
                              cv::Point2d* imageStart,
                              cv::Point2d* imageEnd,
                              const QString& coordinateSpace,
                              const QPointF& partStartRecipe,
                              const QPointF& partEndRecipe,
                              const QPointF& imageStartRecipe,
                              const QPointF& imageEndRecipe);

CenterGuideRecipeCoords centerGuideFromRuntime(bool hasPartGuide,
                                               const cv::Point2d& partCenter,
                                               bool hasImageGuide,
                                               const cv::Point2d& imageCenter);

void applyCenterGuideRuntime(bool* hasPartGuide,
                             bool* hasImageGuide,
                             cv::Point2d* partCenter,
                             cv::Point2d* imageCenter,
                             const QString& coordinateSpace,
                             const QPointF& partCenterRecipe,
                             const QPointF& imageCenterRecipe);

ArcGuideRecipeCoords arcGuideFromRuntime(bool hasPartGuide,
                                         const cv::Point2d& partCenter,
                                         const cv::Point2d& partStart,
                                         const cv::Point2d& partEnd,
                                         bool hasPartThrough,
                                         const cv::Point2d& partThrough,
                                         bool hasImageGuide,
                                         const cv::Point2d& imageCenter,
                                         const cv::Point2d& imageStart,
                                         const cv::Point2d& imageEnd,
                                         bool hasImageThrough,
                                         const cv::Point2d& imageThrough);

void applyArcGuideRuntime(bool* hasPartGuide,
                          bool* hasImageGuide,
                          cv::Point2d* partCenter,
                          cv::Point2d* partStart,
                          cv::Point2d* partEnd,
                          cv::Point2d* partThrough,
                          cv::Point2d* imageCenter,
                          cv::Point2d* imageStart,
                          cv::Point2d* imageEnd,
                          cv::Point2d* imageThrough,
                          const QString& coordinateSpace,
                          const QPointF& partCenterRecipe,
                          const QPointF& partStartRecipe,
                          const QPointF& partEndRecipe,
                          const QPointF& partThroughRecipe,
                          const QPointF& imageCenterRecipe,
                          const QPointF& imageStartRecipe,
                          const QPointF& imageEndRecipe,
                          const QPointF& imageThroughRecipe);
}

template<typename Config>
QString GeometryRecipePersistence::nextGeometryId(const QString& prefix, const QVector<Config>& configs)
{
  QStringList existingIds;
  existingIds.reserve(configs.size());
  for (const Config& config : configs)
  {
    existingIds.append(config.id);
  }
  return RecipeJsonUtils::nextPrefixedId(prefix, existingIds);
}
