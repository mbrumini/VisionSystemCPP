#pragma once

#include "gui/geometry/GeometryOverlay.h"

#include <opencv2/core/types.hpp>

namespace GeometryDiagnosticDrawing
{
void drawCyanPointCross(cv::Mat& image, const cv::Point2d& point);
void appendCyanPointCross(GeometryOverlay& overlay, const cv::Point2d& point, bool compact = false);
void drawOrangePointCross(cv::Mat& image, const cv::Point2d& point);
void appendOrangePointCross(GeometryOverlay& overlay, const cv::Point2d& point, bool compact = false);
}
