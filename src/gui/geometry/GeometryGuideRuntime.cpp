#include "gui/geometry/GeometryGuideRuntime.h"

#include "gui/geometry/ArcGuideMath.h"

namespace GeometryGuideRuntime
{
bool shouldUsePartGuide(bool poseValid,
                        bool hasPartGuide,
                        bool hasImageGuide,
                        bool anchorInImageSpace,
                        const QSize& referenceSize,
                        const cv::Size& imageSize)
{
  if (anchorInImageSpace || !poseValid || !hasPartGuide)
  {
    return false;
  }

  if (hasImageGuide && referenceSize.isValid() && referenceSize.width() > 0 && referenceSize.height() > 0 &&
      (referenceSize.width() != imageSize.width || referenceSize.height() != imageSize.height))
  {
    return false;
  }

  return true;
}

cv::Point2d mapImageGuidePoint(const cv::Point2d& imagePoint,
                               const QSize& referenceSize,
                               const cv::Size& imageSize)
{
  if (!referenceSize.isValid() || referenceSize.width() <= 0 || referenceSize.height() <= 0 ||
      imageSize.width <= 0 || imageSize.height <= 0 ||
      (referenceSize.width() == imageSize.width && referenceSize.height() == imageSize.height))
  {
    return imagePoint;
  }

  const double scaleX = static_cast<double>(imageSize.width) / static_cast<double>(referenceSize.width());
  const double scaleY = static_cast<double>(imageSize.height) / static_cast<double>(referenceSize.height());
  return {imagePoint.x * scaleX, imagePoint.y * scaleY};
}

double guideImageScale(const QSize& referenceSize, const cv::Size& imageSize)
{
  if (!referenceSize.isValid() || referenceSize.width() <= 0 || referenceSize.height() <= 0 ||
      imageSize.width <= 0 || imageSize.height <= 0 ||
      (referenceSize.width() == imageSize.width && referenceSize.height() == imageSize.height))
  {
    return 1.0;
  }

  return 0.5 *
         (static_cast<double>(imageSize.width) / static_cast<double>(referenceSize.width()) +
          static_cast<double>(imageSize.height) / static_cast<double>(referenceSize.height()));
}

double mapImageGuideRadius(double radius, const QSize& referenceSize, const cv::Size& imageSize)
{
  return radius * guideImageScale(referenceSize, imageSize);
}

cv::Point2d resolveImagePoint(const PartPose& pose,
                              bool usePartGuide,
                              const cv::Point2d& partPoint,
                              const cv::Point2d& imagePoint,
                              const QSize& referenceSize,
                              const cv::Size& imageSize)
{
  if (usePartGuide)
  {
    return partToImage(pose, partPoint);
  }

  return mapImageGuidePoint(imagePoint, referenceSize, imageSize);
}

void syncPartGuidesFromImage(const PartPose& pose,
                             QVector<GeometryLineRuntimeConfig>& lines,
                             QVector<GeometryPointRuntimeConfig>& points,
                             QVector<GeometryCircleRuntimeConfig>& circles,
                             QVector<GeometryArcRuntimeConfig>& arcs)
{
  if (!pose.valid)
  {
    return;
  }

  for (GeometryLineRuntimeConfig& line : lines)
  {
    if (line.anchorInImageSpace || !line.hasImageLine || line.hasLine)
    {
      continue;
    }
    line.partStart = imageToPart(pose, line.imageStart);
    line.partEnd = imageToPart(pose, line.imageEnd);
    line.hasLine = true;
  }

  for (GeometryPointRuntimeConfig& point : points)
  {
    if (point.anchorInImageSpace || !point.hasImageGuide || point.hasGuide)
    {
      continue;
    }
    point.partStart = imageToPart(pose, point.imageStart);
    point.partEnd = imageToPart(pose, point.imageEnd);
    point.hasGuide = true;
  }

  for (GeometryCircleRuntimeConfig& circle : circles)
  {
    if (circle.anchorInImageSpace || !circle.hasImageCircle || circle.hasCircle)
    {
      continue;
    }
    circle.partCenter = imageToPart(pose, circle.imageCenter);
    circle.hasCircle = true;
  }

  for (GeometryArcRuntimeConfig& arc : arcs)
  {
    if (arc.anchorInImageSpace || !arc.hasImageArc || arc.hasArc)
    {
      continue;
    }
    arc.partCenter = imageToPart(pose, arc.imageCenter);
    arc.partStart = imageToPart(pose, arc.imageStart);
    arc.partEnd = imageToPart(pose, arc.imageEnd);
    if (arc.hasImageThrough)
    {
      arc.partThrough = imageToPart(pose, arc.imageThrough);
    }
    arc.hasArc = true;
    ArcGuideMath::syncArcPartAngles(arc);
  }
}

void forceSyncPartGuidesFromImage(const PartPose& pose,
                                 QVector<GeometryLineRuntimeConfig>& lines,
                                 QVector<GeometryPointRuntimeConfig>& points,
                                 QVector<GeometryCircleRuntimeConfig>& circles,
                                 QVector<GeometryArcRuntimeConfig>& arcs)
{
  if (!pose.valid)
  {
    return;
  }

  for (GeometryLineRuntimeConfig& line : lines)
  {
    if (line.anchorInImageSpace || !line.hasImageLine)
    {
      continue;
    }
    line.partStart = imageToPart(pose, line.imageStart);
    line.partEnd = imageToPart(pose, line.imageEnd);
    line.hasLine = true;
  }

  for (GeometryPointRuntimeConfig& point : points)
  {
    if (point.anchorInImageSpace || !point.hasImageGuide)
    {
      continue;
    }
    point.partStart = imageToPart(pose, point.imageStart);
    point.partEnd = imageToPart(pose, point.imageEnd);
    point.hasGuide = true;
  }

  for (GeometryCircleRuntimeConfig& circle : circles)
  {
    if (circle.anchorInImageSpace || !circle.hasImageCircle)
    {
      continue;
    }
    circle.partCenter = imageToPart(pose, circle.imageCenter);
    circle.hasCircle = true;
  }

  for (GeometryArcRuntimeConfig& arc : arcs)
  {
    if (arc.anchorInImageSpace || !arc.hasImageArc)
    {
      continue;
    }
    arc.partCenter = imageToPart(pose, arc.imageCenter);
    arc.partStart = imageToPart(pose, arc.imageStart);
    arc.partEnd = imageToPart(pose, arc.imageEnd);
    if (arc.hasImageThrough)
    {
      arc.partThrough = imageToPart(pose, arc.imageThrough);
    }
    arc.hasArc = true;
    ArcGuideMath::syncArcPartAngles(arc);
  }
}
}
