#pragma once

#include "gui/geometry/GeometryOverlay.h"

#include <QColor>

#include <opencv2/core.hpp>

void appendGeometryCirclePolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  const QColor& color,
  int width);

void appendGeometryArcPolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  double startAngle,
  double endAngle,
  const QColor& color,
  int width);
