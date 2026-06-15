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
