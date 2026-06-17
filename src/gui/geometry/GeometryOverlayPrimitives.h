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

void appendGeometryCircleSearchBandGuides(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double innerRadius,
  double outerRadius,
  bool scanOutward,
  const QColor& color,
  int width = 2);

void appendGeometryArcSearchBandGuides(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double innerRadius,
  double outerRadius,
  double startAngle,
  double endAngle,
  bool scanOutward,
  const QColor& color,
  int width = 2);

void appendGeometrySegmentSearchBandGuides(
  GeometryOverlay& overlay,
  const QPointF& start,
  const QPointF& end,
  double bandHalfWidth,
  bool scanNormalPositive,
  const QColor& color,
  int width = 2);

void appendGeometrySegmentDirectionArrow(
  GeometryOverlay& overlay,
  const QPointF& start,
  const QPointF& end,
  const QColor& color,
  int width = 2);
