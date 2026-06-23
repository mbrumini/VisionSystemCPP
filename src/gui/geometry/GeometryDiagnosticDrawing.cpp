#include "GeometryDiagnosticDrawing.h"

#include <QColor>

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace GeometryDiagnosticDrawing
{
namespace
{
void drawPointCross(cv::Mat& image, const cv::Point2d& point, const cv::Scalar& color, int width)
{
  const cv::Point center(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)));
  const int size = 10;
  cv::line(
    image,
    cv::Point(center.x - size, center.y - size),
    cv::Point(center.x + size, center.y + size),
    color,
    width,
    cv::LINE_AA);
  cv::line(
    image,
    cv::Point(center.x - size, center.y + size),
    cv::Point(center.x + size, center.y - size),
    color,
    width,
    cv::LINE_AA);
}

void appendPointCross(GeometryOverlay& overlay, const cv::Point2d& point, const QColor& color, int width, double size)
{
  const QPointF center(point.x, point.y);
  overlay.lines.append({
    QPointF(center.x() - size, center.y() - size),
    QPointF(center.x() + size, center.y() + size),
    color,
    width
  });
  overlay.lines.append({
    QPointF(center.x() - size, center.y() + size),
    QPointF(center.x() + size, center.y() - size),
    color,
    width
  });
}
}

void drawCyanPointCross(cv::Mat& image, const cv::Point2d& point)
{
  drawPointCross(image, point, cv::Scalar(255, 255, 0), 3);
}

void appendCyanPointCross(GeometryOverlay& overlay, const cv::Point2d& point, bool compact)
{
  appendPointCross(
    overlay,
    point,
    QColor("#00d2ff"),
    compact ? 2 : 4,
    compact ? 6.0 : 10.0);
}

void drawOrangePointCross(cv::Mat& image, const cv::Point2d& point)
{
  drawPointCross(image, point, cv::Scalar(0, 140, 255), 4);
}

void appendOrangePointCross(GeometryOverlay& overlay, const cv::Point2d& point, bool compact)
{
  appendPointCross(
    overlay,
    point,
    QColor("#ff8a00"),
    compact ? 2 : 6,
    compact ? 6.0 : 10.0);
}
}
