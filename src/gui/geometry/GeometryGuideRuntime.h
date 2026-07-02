#pragma once

#include "gui/geometry/GeometryRuntimeConfig.h"
#include "runtime/PartPose.h"

#include <QSize>
#include <QVector>

#include <opencv2/core/types.hpp>

namespace GeometryGuideRuntime
{
bool shouldUsePartGuide(bool poseValid,
                        bool hasPartGuide,
                        bool hasImageGuide,
                        bool anchorInImageSpace,
                        const QSize& referenceSize,
                        const cv::Size& imageSize);

cv::Point2d mapImageGuidePoint(const cv::Point2d& imagePoint,
                               const QSize& referenceSize,
                               const cv::Size& imageSize);

double guideImageScale(const QSize& referenceSize, const cv::Size& imageSize);

double mapImageGuideRadius(double radius, const QSize& referenceSize, const cv::Size& imageSize);

cv::Point2d resolveImagePoint(const PartPose& pose,
                              bool usePartGuide,
                              const cv::Point2d& partPoint,
                              const cv::Point2d& imagePoint,
                              const QSize& referenceSize,
                              const cv::Size& imageSize);

void syncPartGuidesFromImage(const PartPose& pose,
                             QVector<GeometryLineRuntimeConfig>& lines,
                             QVector<GeometryPointRuntimeConfig>& points,
                             QVector<GeometryCircleRuntimeConfig>& circles,
                             QVector<GeometryArcRuntimeConfig>& arcs);

void forceSyncPartGuidesFromImage(const PartPose& pose,
                                 QVector<GeometryLineRuntimeConfig>& lines,
                                 QVector<GeometryPointRuntimeConfig>& points,
                                 QVector<GeometryCircleRuntimeConfig>& circles,
                                 QVector<GeometryArcRuntimeConfig>& arcs);
}
