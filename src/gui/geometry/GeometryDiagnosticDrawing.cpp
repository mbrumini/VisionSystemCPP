#include "GeometryDiagnosticDrawing.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace GeometryDiagnosticDrawing
{
void drawCyanPointCross(cv::Mat& image, const cv::Point2d& point)
{
  const cv::Point center(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)));
  const int size = 8;
  const cv::Scalar cyan(255, 255, 0);
  cv::line(
    image,
    cv::Point(center.x - size, center.y - size),
    cv::Point(center.x + size, center.y + size),
    cyan,
    2,
    cv::LINE_AA);
  cv::line(
    image,
    cv::Point(center.x - size, center.y + size),
    cv::Point(center.x + size, center.y - size),
    cyan,
    2,
    cv::LINE_AA);
}

void appendCyanPointCross(GeometryOverlay& overlay, const cv::Point2d& point)
{
  const QPointF center(point.x, point.y);
  constexpr double size = 8.0;
  const QColor cyan("#00d2ff");
  overlay.lines.append({
    QPointF(center.x() - size, center.y() - size),
    QPointF(center.x() + size, center.y() + size),
    cyan,
    3
  });
  overlay.lines.append({
    QPointF(center.x() - size, center.y() + size),
    QPointF(center.x() + size, center.y() - size),
    cyan,
    3
  });
}
}
