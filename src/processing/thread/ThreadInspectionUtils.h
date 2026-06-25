#pragma once

#include "config/GeometryRecipeJson.h"
#include "config/RecipeManager.h"
#include "runtime/PartPose.h"

#include <QRect>
#include <QSize>

inline QRect effectiveThreadExtractionImageRect(
  const ThreadInspectionSettings& settings,
  const PartPose& pose,
  const QSize& imageSize)
{
  const QRect imageBounds(0, 0, imageSize.width(), imageSize.height());
  if (!settings.hasExtractionRoi)
  {
    return {};
  }

  const bool usePartSpace =
    pose.valid &&
    settings.partRoi.isValid() &&
    !GeometryRecipeJson::isImageSpace(settings.coordinateSpace);

  if (usePartSpace)
  {
    return partRectToImageBoundingRect(pose, settings.partRoi.normalized()).intersected(imageBounds);
  }

  if (settings.imageRoi.isValid())
  {
    return settings.imageRoi.normalized().intersected(imageBounds);
  }

  if (pose.valid && settings.partRoi.isValid())
  {
    return partRectToImageBoundingRect(pose, settings.partRoi.normalized()).intersected(imageBounds);
  }

  return {};
}
