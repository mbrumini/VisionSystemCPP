#include "gui/geometry/GeometryOverlayPrimitives.h"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>

void appendGeometryCirclePolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  const QColor& color,
  int width)
{
  if (radius <= 1.0)
  {
    return;
  }

  constexpr int segments = 96;
  QPointF previous(center.x + radius, center.y);
  for (int i = 1; i <= segments; ++i)
  {
    const double angle = 2.0 * CV_PI * static_cast<double>(i) / static_cast<double>(segments);
    const QPointF current(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    overlay.lines.append({previous, current, color, width});
    previous = current;
  }
}

void appendGeometryArcPolyline(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double radius,
  double startAngle,
  double endAngle,
  const QColor& color,
  int width)
{
  if (radius <= 1.0)
  {
    return;
  }

  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  if (span < 0.001)
  {
    span = 2.0 * CV_PI;
  }

  const int segments = std::max(8, static_cast<int>(std::round(64.0 * span / (2.0 * CV_PI))));
  QPointF previous;
  for (int i = 0; i <= segments; ++i)
  {
    const double angle = startAngle + span * static_cast<double>(i) / static_cast<double>(segments);
    const QPointF current(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    if (i > 0)
    {
      overlay.lines.append({previous, current, color, width});
    }
    previous = current;
  }
}

void appendGeometryCircleSearchBandGuides(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double innerRadius,
  double outerRadius,
  bool scanOutward,
  const QColor& color,
  int width)
{
  if (innerRadius <= 1.0 || outerRadius <= innerRadius + 1.0)
  {
    return;
  }

  const double bandWidth = outerRadius - innerRadius;
  const double arrowLength = std::clamp(bandWidth * 0.28, 7.0, 18.0);
  const double arrowHalfWidth = std::clamp(bandWidth * 0.14, 4.0, 10.0);
  const int guideCount = std::clamp(static_cast<int>(std::round(outerRadius / 36.0)), 12, 32);
  const double arrowRadius = innerRadius + bandWidth * (scanOutward ? 0.62 : 0.38);

  for (int i = 0; i < guideCount; ++i)
  {
    const double angle = 2.0 * CV_PI * static_cast<double>(i) / static_cast<double>(guideCount);
    const QPointF radial(std::cos(angle), std::sin(angle));
    const QPointF tangent(-radial.y(), radial.x());
    const QPointF inner(center.x + radial.x() * innerRadius, center.y + radial.y() * innerRadius);
    const QPointF outer(center.x + radial.x() * outerRadius, center.y + radial.y() * outerRadius);
    overlay.lines.append({inner, outer, color, width});

    const QPointF direction = scanOutward ? radial : QPointF(-radial.x(), -radial.y());
    const QPointF tip(center.x + radial.x() * arrowRadius, center.y + radial.y() * arrowRadius);
    const QPointF base = tip - direction * arrowLength;
    overlay.lines.append({base + tangent * arrowHalfWidth, tip, color, width});
    overlay.lines.append({base - tangent * arrowHalfWidth, tip, color, width});
  }
}

void appendGeometryArcSearchBandGuides(
  GeometryOverlay& overlay,
  const cv::Point2d& center,
  double innerRadius,
  double outerRadius,
  double startAngle,
  double endAngle,
  bool scanOutward,
  const QColor& color,
  int width)
{
  if (innerRadius <= 1.0 || outerRadius <= innerRadius + 1.0)
  {
    return;
  }

  double span = endAngle - startAngle;
  while (span < 0.0)
  {
    span += 2.0 * CV_PI;
  }
  if (span < 0.001)
  {
    span = 2.0 * CV_PI;
  }

  const double bandWidth = outerRadius - innerRadius;
  const double arrowLength = std::clamp(bandWidth * 0.28, 7.0, 18.0);
  const double arrowHalfWidth = std::clamp(bandWidth * 0.14, 4.0, 10.0);
  const int guideCount = std::clamp(static_cast<int>(std::round(span * outerRadius / 140.0)) + 2, 3, 24);
  const double arrowRadius = innerRadius + bandWidth * (scanOutward ? 0.62 : 0.38);

  for (int i = 0; i < guideCount; ++i)
  {
    const double t = guideCount == 1 ? 0.5 : static_cast<double>(i) / static_cast<double>(guideCount - 1);
    const double angle = startAngle + span * t;
    const QPointF radial(std::cos(angle), std::sin(angle));
    const QPointF tangent(-radial.y(), radial.x());
    const QPointF inner(center.x + radial.x() * innerRadius, center.y + radial.y() * innerRadius);
    const QPointF outer(center.x + radial.x() * outerRadius, center.y + radial.y() * outerRadius);
    overlay.lines.append({inner, outer, color, width});

    const QPointF direction = scanOutward ? radial : QPointF(-radial.x(), -radial.y());
    const QPointF tip(center.x + radial.x() * arrowRadius, center.y + radial.y() * arrowRadius);
    const QPointF base = tip - direction * arrowLength;
    overlay.lines.append({base + tangent * arrowHalfWidth, tip, color, width});
    overlay.lines.append({base - tangent * arrowHalfWidth, tip, color, width});
  }
}

void appendGeometrySegmentDirectionArrow(
  GeometryOverlay& overlay,
  const QPointF& start,
  const QPointF& end,
  const QColor& color,
  int width)
{
  const QPointF delta = end - start;
  const double length = std::hypot(delta.x(), delta.y());
  if (length < 1.0)
  {
    return;
  }

  const QPointF unit(delta.x() / length, delta.y() / length);
  const QPointF normal(-unit.y(), unit.x());
  const double arrowLength = std::clamp(length * 0.08, 8.0, 18.0);
  const double arrowHalfWidth = std::clamp(length * 0.035, 4.0, 9.0);
  const QPointF tip = start + unit * (length * 0.56);
  const QPointF base = tip - unit * arrowLength;
  overlay.lines.append({base + normal * arrowHalfWidth, tip, color, width});
  overlay.lines.append({base - normal * arrowHalfWidth, tip, color, width});
}

void appendGeometrySegmentSearchBandGuides(
  GeometryOverlay& overlay,
  const QPointF& start,
  const QPointF& end,
  double bandHalfWidth,
  bool scanNormalPositive,
  const QColor& color,
  int width)
{
  const QPointF delta = end - start;
  const double length = std::hypot(delta.x(), delta.y());
  if (length < 1.0 || bandHalfWidth <= 1.0)
  {
    return;
  }

  const QPointF unit(delta.x() / length, delta.y() / length);
  const QPointF normal(-unit.y(), unit.x());
  const QPointF scanNormal = scanNormalPositive ? normal : QPointF(-normal.x(), -normal.y());
  const int guideCount = std::clamp(static_cast<int>(std::round(length / 140.0)) + 1, 2, 8);
  for (int i = 0; i < guideCount; ++i)
  {
    const double t = guideCount == 1 ? 0.5 : static_cast<double>(i) / static_cast<double>(guideCount - 1);
    const QPointF center = start + delta * t;
    overlay.lines.append({
      center - normal * bandHalfWidth,
      center + normal * bandHalfWidth,
      color,
      width
    });
  }

  const QPointF arrowCenter = start + delta * 0.5;
  appendGeometrySegmentDirectionArrow(
    overlay,
    arrowCenter - scanNormal * (bandHalfWidth * 0.55),
    arrowCenter + scanNormal * (bandHalfWidth * 0.55),
    color,
    width);
}
