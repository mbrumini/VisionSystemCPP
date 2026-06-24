#include "gui/geometry/GeometryRecipePersistence.h"

#include "config/GeometryRecipeJson.h"

namespace GeometryRecipePersistence
{
QString scanDirectionToRecipe(EdgeLineScanDirection direction)
{
  return direction == EdgeLineScanDirection::NormalNegative ? "normal_negative" : "normal_positive";
}

EdgeLineScanDirection scanDirectionFromRecipe(const QString& value)
{
  return value == "normal_negative" ? EdgeLineScanDirection::NormalNegative : EdgeLineScanDirection::NormalPositive;
}

QString transitionToRecipe(EdgeLineTransition transition)
{
  return transition == EdgeLineTransition::DarkToLight ? "dark_to_light" : "light_to_dark";
}

EdgeLineTransition transitionFromRecipe(const QString& value)
{
  return value == "dark_to_light" ? EdgeLineTransition::DarkToLight : EdgeLineTransition::LightToDark;
}

QString pickModeToRecipe(EdgeLinePickMode mode)
{
  if (mode == EdgeLinePickMode::Last)
  {
    return "last";
  }

  if (mode == EdgeLinePickMode::Best)
  {
    return "best";
  }

  return "first";
}

EdgeLinePickMode pickModeFromRecipe(const QString& value)
{
  if (value == "last")
  {
    return EdgeLinePickMode::Last;
  }

  if (value == "best")
  {
    return EdgeLinePickMode::Best;
  }

  return EdgeLinePickMode::First;
}

static QPointF toQPointF(const cv::Point2d& point)
{
  return QPointF(point.x, point.y);
}

SegmentGuideRecipeCoords segmentGuideFromRuntime(bool hasPartGuide,
                                                 const cv::Point2d& partStart,
                                                 const cv::Point2d& partEnd,
                                                 bool hasImageGuide,
                                                 const cv::Point2d& imageStart,
                                                 const cv::Point2d& imageEnd)
{
  SegmentGuideRecipeCoords coords;
  if (hasPartGuide)
  {
    coords.coordinateSpace = GeometryRecipeJson::kPartSpace;
    coords.partStart = toQPointF(partStart);
    coords.partEnd = toQPointF(partEnd);
  }
  else
  {
    coords.coordinateSpace = GeometryRecipeJson::kImageSpace;
    coords.imageStart = toQPointF(imageStart);
    coords.imageEnd = toQPointF(imageEnd);
  }
  Q_UNUSED(hasImageGuide);
  return coords;
}

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
                              const QPointF& imageEndRecipe)
{
  if (GeometryRecipeJson::isImageSpace(coordinateSpace))
  {
    *hasPartGuide = false;
    *hasImageGuide = true;
    *imageStart = cv::Point2d(imageStartRecipe.x(), imageStartRecipe.y());
    *imageEnd = cv::Point2d(imageEndRecipe.x(), imageEndRecipe.y());
    return;
  }

  *hasPartGuide = true;
  *hasImageGuide = false;
  *partStart = cv::Point2d(partStartRecipe.x(), partStartRecipe.y());
  *partEnd = cv::Point2d(partEndRecipe.x(), partEndRecipe.y());
}

CenterGuideRecipeCoords centerGuideFromRuntime(bool hasPartGuide,
                                               const cv::Point2d& partCenter,
                                               bool hasImageGuide,
                                               const cv::Point2d& imageCenter)
{
  CenterGuideRecipeCoords coords;
  if (hasPartGuide)
  {
    coords.coordinateSpace = GeometryRecipeJson::kPartSpace;
    coords.partCenter = toQPointF(partCenter);
  }
  else
  {
    coords.coordinateSpace = GeometryRecipeJson::kImageSpace;
    coords.imageCenter = toQPointF(imageCenter);
  }
  Q_UNUSED(hasImageGuide);
  return coords;
}

void applyCenterGuideRuntime(bool* hasPartGuide,
                             bool* hasImageGuide,
                             cv::Point2d* partCenter,
                             cv::Point2d* imageCenter,
                             const QString& coordinateSpace,
                             const QPointF& partCenterRecipe,
                             const QPointF& imageCenterRecipe)
{
  if (GeometryRecipeJson::isImageSpace(coordinateSpace))
  {
    *hasPartGuide = false;
    *hasImageGuide = true;
    *imageCenter = cv::Point2d(imageCenterRecipe.x(), imageCenterRecipe.y());
    return;
  }

  *hasPartGuide = true;
  *hasImageGuide = false;
  *partCenter = cv::Point2d(partCenterRecipe.x(), partCenterRecipe.y());
}

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
                                         const cv::Point2d& imageThrough)
{
  ArcGuideRecipeCoords coords;
  if (hasPartGuide)
  {
    coords.coordinateSpace = GeometryRecipeJson::kPartSpace;
    coords.partCenter = toQPointF(partCenter);
    coords.partStart = toQPointF(partStart);
    coords.partEnd = toQPointF(partEnd);
    if (hasPartThrough)
    {
      coords.partThrough = toQPointF(partThrough);
    }
  }
  if (hasImageGuide)
  {
    if (!hasPartGuide)
    {
      coords.coordinateSpace = GeometryRecipeJson::kImageSpace;
    }
    coords.imageCenter = toQPointF(imageCenter);
    coords.imageStart = toQPointF(imageStart);
    coords.imageEnd = toQPointF(imageEnd);
    if (hasImageThrough)
    {
      coords.imageThrough = toQPointF(imageThrough);
    }
  }
  return coords;
}

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
                          const QPointF& imageThroughRecipe)
{
  const bool hasImageCoords =
    !imageCenterRecipe.isNull() || !imageStartRecipe.isNull() || !imageEndRecipe.isNull();
  const bool hasPartCoords =
    !partCenterRecipe.isNull() || !partStartRecipe.isNull() || !partEndRecipe.isNull();

  if (hasImageCoords || GeometryRecipeJson::isImageSpace(coordinateSpace))
  {
    *hasImageGuide = true;
    if (!imageCenterRecipe.isNull())
    {
      *imageCenter = cv::Point2d(imageCenterRecipe.x(), imageCenterRecipe.y());
    }
    if (!imageStartRecipe.isNull())
    {
      *imageStart = cv::Point2d(imageStartRecipe.x(), imageStartRecipe.y());
    }
    if (!imageEndRecipe.isNull())
    {
      *imageEnd = cv::Point2d(imageEndRecipe.x(), imageEndRecipe.y());
    }
    if (!imageThroughRecipe.isNull())
    {
      *imageThrough = cv::Point2d(imageThroughRecipe.x(), imageThroughRecipe.y());
    }
  }
  else
  {
    *hasImageGuide = false;
  }

  if (hasPartCoords || !GeometryRecipeJson::isImageSpace(coordinateSpace))
  {
    *hasPartGuide = true;
    if (!partCenterRecipe.isNull())
    {
      *partCenter = cv::Point2d(partCenterRecipe.x(), partCenterRecipe.y());
    }
    if (!partStartRecipe.isNull())
    {
      *partStart = cv::Point2d(partStartRecipe.x(), partStartRecipe.y());
    }
    if (!partEndRecipe.isNull())
    {
      *partEnd = cv::Point2d(partEndRecipe.x(), partEndRecipe.y());
    }
    if (!partThroughRecipe.isNull())
    {
      *partThrough = cv::Point2d(partThroughRecipe.x(), partThroughRecipe.y());
    }
  }
  else
  {
    *hasPartGuide = false;
  }
}
}
