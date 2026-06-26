#pragma once

#include "config/GeometryRecipeJson.h"
#include "config/RecipeManager.h"
#include "runtime/PartPose.h"

#include <opencv2/core.hpp>

#include <QRect>
#include <QSize>

#include <array>
#include <cmath>

inline std::array<cv::Point2f, 4> partRectToImageQuad(const PartPose& pose, const QRect& partRect)
{
  const auto toImage = [&](int x, int y) {
    const cv::Point2d image = partToImage(pose, cv::Point2d(x, y));
    return cv::Point2f(static_cast<float>(image.x), static_cast<float>(image.y));
  };

  return {
    toImage(partRect.left(), partRect.top()),
    toImage(partRect.right(), partRect.top()),
    toImage(partRect.right(), partRect.bottom()),
    toImage(partRect.left(), partRect.bottom())
  };
}

inline bool usePartSpaceThreadRoi(const ThreadInspectionSettings& settings, const PartPose& pose)
{
  return pose.valid &&
         settings.hasExtractionRoi &&
         settings.partRoi.isValid() &&
         !GeometryRecipeJson::isImageSpace(settings.coordinateSpace);
}

inline bool canExtractThreadProfile(const ThreadInspectionSettings& settings, const PartPose& pose)
{
  if (!settings.hasExtractionRoi)
  {
    return false;
  }

  if (pose.valid)
  {
    return true;
  }

  return settings.imageRoi.isValid();
}

inline std::array<cv::Point2f, 4> effectiveThreadExtractionImageQuad(
  const ThreadInspectionSettings& settings,
  const PartPose& pose)
{
  if (usePartSpaceThreadRoi(settings, pose))
  {
    return partRectToImageQuad(pose, settings.partRoi.normalized());
  }

  if (pose.valid && settings.partRoi.isValid())
  {
    return partRectToImageQuad(pose, settings.partRoi.normalized());
  }

  const QRect imageRoi = settings.imageRoi.normalized();
  return {
    cv::Point2f(static_cast<float>(imageRoi.left()), static_cast<float>(imageRoi.top())),
    cv::Point2f(static_cast<float>(imageRoi.right()), static_cast<float>(imageRoi.top())),
    cv::Point2f(static_cast<float>(imageRoi.right()), static_cast<float>(imageRoi.bottom())),
    cv::Point2f(static_cast<float>(imageRoi.left()), static_cast<float>(imageRoi.bottom()))
  };
}

inline QSize effectiveThreadExtractionPartSize(
  const ThreadInspectionSettings& settings,
  const PartPose& pose)
{
  if (usePartSpaceThreadRoi(settings, pose) || (pose.valid && settings.partRoi.isValid()))
  {
    return settings.partRoi.normalized().size();
  }

  return settings.imageRoi.normalized().size();
}

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

  const std::array<cv::Point2f, 4> quad = effectiveThreadExtractionImageQuad(settings, pose);
  double minX = quad.front().x;
  double maxX = quad.front().x;
  double minY = quad.front().y;
  double maxY = quad.front().y;
  for (const cv::Point2f& point : quad)
  {
    minX = std::min<double>(minX, point.x);
    maxX = std::max<double>(maxX, point.x);
    minY = std::min<double>(minY, point.y);
    maxY = std::max<double>(maxY, point.y);
  }

  return QRect(
           static_cast<int>(std::floor(minX)),
           static_cast<int>(std::floor(minY)),
           static_cast<int>(std::ceil(maxX - minX)),
           static_cast<int>(std::ceil(maxY - minY)))
    .intersected(imageBounds);
}

inline cv::Point2f threadWarpedPointToImage(
  const std::array<cv::Point2f, 4>& quad,
  double warpedColumn,
  double warpedRow,
  int warpedWidth,
  int warpedHeight)
{
  const double u = warpedWidth > 1 ? warpedColumn / static_cast<double>(warpedWidth - 1) : 0.0;
  const double v = warpedHeight > 1 ? warpedRow / static_cast<double>(warpedHeight - 1) : 0.0;
  const cv::Point2f top = quad[0] * static_cast<float>(1.0 - u) + quad[1] * static_cast<float>(u);
  const cv::Point2f bottom = quad[3] * static_cast<float>(1.0 - u) + quad[2] * static_cast<float>(u);
  return top * static_cast<float>(1.0 - v) + bottom * static_cast<float>(v);
}
