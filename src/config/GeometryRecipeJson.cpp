#include "config/GeometryRecipeJson.h"

#include "RecipeJsonUtils.h"

namespace GeometryRecipeJson
{
bool isImageSpace(const QString& coordinateSpace)
{
  return coordinateSpace == kImageSpace;
}

QString normalizedCoordinateSpace(const QString& coordinateSpace)
{
  return isImageSpace(coordinateSpace) ? QString::fromLatin1(kImageSpace) : QString::fromLatin1(kPartSpace);
}

bool segmentRecipeValid(const QString& coordinateSpace,
                        const QPointF& partStart,
                        const QPointF& partEnd,
                        const QPointF& imageStart,
                        const QPointF& imageEnd)
{
  if (isImageSpace(coordinateSpace))
  {
    return !imageStart.isNull() || !imageEnd.isNull();
  }

  return !partStart.isNull() || !partEnd.isNull();
}

bool centerRecipeValid(const QString& coordinateSpace,
                       const QPointF& partCenter,
                       const QPointF& imageCenter)
{
  if (isImageSpace(coordinateSpace))
  {
    return !imageCenter.isNull();
  }

  return !partCenter.isNull();
}

bool arcRecipeValid(const QString& coordinateSpace,
                    const QPointF& partCenter,
                    const QPointF& partStart,
                    const QPointF& partEnd,
                    const QPointF& imageCenter,
                    const QPointF& imageStart,
                    const QPointF& imageEnd)
{
  if (isImageSpace(coordinateSpace))
  {
    return !imageCenter.isNull() || !imageStart.isNull() || !imageEnd.isNull();
  }

  return !partCenter.isNull() || !partStart.isNull() || !partEnd.isNull();
}

void writeSegmentCoordinates(QJsonObject& object,
                           const QString& coordinateSpace,
                           const QPointF& partStart,
                           const QPointF& partEnd,
                           const QPointF& imageStart,
                           const QPointF& imageEnd)
{
  object["coordinateSpace"] = normalizedCoordinateSpace(coordinateSpace);
  if (isImageSpace(coordinateSpace))
  {
    object["imageStart"] = RecipeJsonUtils::pointFToJson(imageStart);
    object["imageEnd"] = RecipeJsonUtils::pointFToJson(imageEnd);
    return;
  }

  object["partStart"] = RecipeJsonUtils::pointFToJson(partStart);
  object["partEnd"] = RecipeJsonUtils::pointFToJson(partEnd);
}

void writeCenterCoordinate(QJsonObject& object,
                           const QString& coordinateSpace,
                           const QPointF& partCenter,
                           const QPointF& imageCenter)
{
  object["coordinateSpace"] = normalizedCoordinateSpace(coordinateSpace);
  if (isImageSpace(coordinateSpace))
  {
    object["imageCenter"] = RecipeJsonUtils::pointFToJson(imageCenter);
    return;
  }

  object["partCenter"] = RecipeJsonUtils::pointFToJson(partCenter);
}

void writeArcCoordinates(QJsonObject& object,
                         const QString& coordinateSpace,
                         const QPointF& partCenter,
                         const QPointF& partStart,
                         const QPointF& partEnd,
                         const QPointF& imageCenter,
                         const QPointF& imageStart,
                         const QPointF& imageEnd)
{
  object["coordinateSpace"] = normalizedCoordinateSpace(coordinateSpace);
  if (isImageSpace(coordinateSpace))
  {
    object["imageCenter"] = RecipeJsonUtils::pointFToJson(imageCenter);
    object["imageStart"] = RecipeJsonUtils::pointFToJson(imageStart);
    object["imageEnd"] = RecipeJsonUtils::pointFToJson(imageEnd);
    return;
  }

  object["partCenter"] = RecipeJsonUtils::pointFToJson(partCenter);
  object["partStart"] = RecipeJsonUtils::pointFToJson(partStart);
  object["partEnd"] = RecipeJsonUtils::pointFToJson(partEnd);
}
}
