#include "ImageViewWidget.h"

#include <QPolygonF>

#include <algorithm>
#include <cmath>

ImageViewWidget::ImageViewWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumSize(320, 240);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void ImageViewWidget::setImage(const QPixmap& image)
{
  m_image = image;
  update();
}

void ImageViewWidget::clearImage()
{
  m_image = {};
  m_zoomFactor = 1.0;
  m_panOffset = {};
  m_hasRoi = false;
  m_hasGeometryArea = false;
  m_exclusionRects.clear();
  m_circles.clear();
  m_geometryPoints.clear();
  m_geometryLines.clear();
  m_geometryOverlay.clear();
  update();
}

void ImageViewWidget::setRoi(const QRect& imageRoi)
{
  m_roi = imageRoi.normalized();
  m_hasRoi = m_roi.isValid();
  update();
}

void ImageViewWidget::clearRoi()
{
  m_hasRoi = false;
  m_roi = {};
  update();
}

void ImageViewWidget::setExclusionRects(const QVector<QRect>& imageRects)
{
  m_exclusionRects.clear();
  m_selectedExclusionIndex = -1;

  for (const QRect& rect : imageRects)
  {
    const QRect normalized = rect.normalized();

    if (normalized.isValid())
    {
      m_exclusionRects.append(normalized);
    }
  }

  update();
}

void ImageViewWidget::clearExclusionRects()
{
  m_exclusionRects.clear();
  m_selectedExclusionIndex = -1;
  update();
}

void ImageViewWidget::setCircles(const QVector<ImageCircle>& imageCircles)
{
  m_circles.clear();

  for (const ImageCircle& circle : imageCircles)
  {
    if (circle.radius > 0)
    {
      m_circles.append(circle);
    }
  }

  update();
}

void ImageViewWidget::clearCircles()
{
  m_circles.clear();
  update();
}

void ImageViewWidget::setGeometryArea(const ImageRotatedRect& imageArea)
{
  m_geometryArea = normalizedGeometryArea(imageArea);
  m_hasGeometryArea = m_geometryArea.size.width() > 2.0 && m_geometryArea.size.height() > 2.0;
  update();
}

void ImageViewWidget::clearGeometryArea()
{
  m_hasGeometryArea = false;
  m_geometryArea = {};
  update();
}

void ImageViewWidget::setGeometryPoints(const QVector<QPointF>& imagePoints)
{
  m_geometryPoints = imagePoints;
  update();
}

void ImageViewWidget::clearGeometryPoints()
{
  m_geometryPoints.clear();
  update();
}

void ImageViewWidget::setGeometryLines(const QVector<ImageLine>& imageLines)
{
  m_geometryLines = imageLines;
  update();
}

void ImageViewWidget::clearGeometryLines()
{
  m_geometryLines.clear();
  update();
}

void ImageViewWidget::setGeometryOverlay(const GeometryOverlay& overlay)
{
  m_geometryOverlay = overlay;
  update();
}

void ImageViewWidget::clearGeometryOverlay()
{
  m_geometryOverlay.clear();
  update();
}

void ImageViewWidget::setRoiDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Roi : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setExclusionDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Exclusion : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setGeometryAreaEditingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::GeometryArea : DrawingMode::None;
  setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_activeGeometryAreaHandle = GeometryAreaHandle::None;
  m_threePointCirclePoints.clear();
  m_twoPointLinePoints.clear();
  update();
}

void ImageViewWidget::setGeometryPointPickingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::GeometryPointPick : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_activeGeometryAreaHandle = GeometryAreaHandle::None;
  m_threePointCirclePoints.clear();
  m_twoPointLinePoints.clear();
  update();
}

void ImageViewWidget::setGeometryOverlayPointEditingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::GeometryOverlayPointEdit : DrawingMode::None;
  setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_activeGeometryAreaHandle = GeometryAreaHandle::None;
  m_threePointCirclePoints.clear();
  m_twoPointLinePoints.clear();
  update();
}

void ImageViewWidget::setTwoPointLineDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::TwoPointLine : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_threePointCirclePoints.clear();
  m_twoPointLinePoints.clear();
  update();
}

void ImageViewWidget::setOuterCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::OuterCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setInnerCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::InnerCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setThreePointCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::ThreePointCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setThreePointArcDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::ThreePointArc : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setRoiChangedHandler(std::function<void(const QRect&)> handler)
{
  m_roiChangedHandler = std::move(handler);
}

void ImageViewWidget::setExclusionRectAddedHandler(std::function<void(const QRect&)> handler)
{
  m_exclusionRectAddedHandler = std::move(handler);
}

void ImageViewWidget::setExclusionRectsChangedHandler(std::function<void(const QVector<QRect>&)> handler)
{
  m_exclusionRectsChangedHandler = std::move(handler);
}

void ImageViewWidget::setCircleChangedHandler(std::function<void(bool, const ImageCircle&)> handler)
{
  m_circleChangedHandler = std::move(handler);
}

void ImageViewWidget::setThreePointCircleHandler(std::function<void(const QVector<QPoint>&)> handler)
{
  m_threePointCircleHandler = std::move(handler);
}

void ImageViewWidget::setGeometryAreaChangedHandler(std::function<void(const ImageRotatedRect&)> handler)
{
  m_geometryAreaChangedHandler = std::move(handler);
}

void ImageViewWidget::setGeometryPointPickedHandler(std::function<void(const QPointF&)> handler)
{
  m_geometryPointPickedHandler = std::move(handler);
}

void ImageViewWidget::setGeometryPointMovedHandler(std::function<void(const QPointF&)> handler)
{
  m_geometryPointMovedHandler = std::move(handler);
}

void ImageViewWidget::setGeometryOverlayPointMovedHandler(std::function<void(int, const QPointF&)> handler)
{
  m_geometryOverlayPointMovedHandler = std::move(handler);
}

void ImageViewWidget::setTwoPointLineHandler(std::function<void(const QVector<QPoint>&)> handler)
{
  m_twoPointLineHandler = std::move(handler);
}

QRect ImageViewWidget::baseImageDrawRect() const
{
  if (m_image.isNull())
  {
    return {};
  }

  const QSize scaledSize = m_image.size().scaled(size(), Qt::KeepAspectRatio);
  const QPoint topLeft(
    (width() - scaledSize.width()) / 2,
    (height() - scaledSize.height()) / 2);
  return QRect(topLeft, scaledSize);
}

QRect ImageViewWidget::imageDrawRect() const
{
  const QRect baseRect = baseImageDrawRect();
  if (baseRect.isEmpty())
  {
    return {};
  }

  const QSize scaledSize(
    std::max(1, static_cast<int>(std::round(baseRect.width() * m_zoomFactor))),
    std::max(1, static_cast<int>(std::round(baseRect.height() * m_zoomFactor))));
  const QPointF center = QPointF(baseRect.center()) + m_panOffset;
  const QPoint topLeft(
    static_cast<int>(std::round(center.x() - scaledSize.width() * 0.5)),
    static_cast<int>(std::round(center.y() - scaledSize.height() * 0.5)));
  return QRect(topLeft, scaledSize);
}

void ImageViewWidget::clampZoomPan()
{
  if (m_image.isNull())
  {
    m_zoomFactor = 1.0;
    m_panOffset = {};
    return;
  }

  m_zoomFactor = std::clamp(m_zoomFactor, 1.0, 20.0);
  if (m_zoomFactor <= 1.0001)
  {
    m_zoomFactor = 1.0;
    m_panOffset = {};
    return;
  }

  const QRect baseRect = baseImageDrawRect();
  const QSizeF scaledSize(baseRect.width() * m_zoomFactor, baseRect.height() * m_zoomFactor);
  QPointF center = QPointF(baseRect.center()) + m_panOffset;
  const QRectF viewport = rect();

  auto clampAxis = [](double value, double halfSize, double viewportSize, double baseCenter) {
    if (halfSize * 2.0 <= viewportSize)
    {
      return baseCenter;
    }
    return std::clamp(value, viewportSize - halfSize, halfSize);
  };

  center.setX(clampAxis(center.x(), scaledSize.width() * 0.5, viewport.width(), baseRect.center().x()));
  center.setY(clampAxis(center.y(), scaledSize.height() * 0.5, viewport.height(), baseRect.center().y()));
  m_panOffset = center - QPointF(baseRect.center());
}

QPoint ImageViewWidget::widgetToImage(const QPoint& widgetPoint) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(m_image.width()) / drawRect.width();
  const double scaleY = static_cast<double>(m_image.height()) / drawRect.height();
  const int x = qBound(0, static_cast<int>((widgetPoint.x() - drawRect.x()) * scaleX), m_image.width() - 1);
  const int y = qBound(0, static_cast<int>((widgetPoint.y() - drawRect.y()) * scaleY), m_image.height() - 1);
  return QPoint(x, y);
}

QRect ImageViewWidget::widgetRectToImageRect(const QRect& widgetRect) const
{
  const QRect clippedWidgetRect = widgetRect.intersected(imageDrawRect());
  const QPoint topLeft = widgetToImage(clippedWidgetRect.topLeft());
  const QPoint bottomRight = widgetToImage(clippedWidgetRect.bottomRight());
  return QRect(topLeft, bottomRight).normalized();
}

QRect ImageViewWidget::imageRectToWidgetRect(const QRect& imageRect) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(drawRect.width()) / m_image.width();
  const double scaleY = static_cast<double>(drawRect.height()) / m_image.height();
  const QPoint topLeft(
    drawRect.x() + static_cast<int>(imageRect.x() * scaleX),
    drawRect.y() + static_cast<int>(imageRect.y() * scaleY));
  const QSize scaledSize(
    static_cast<int>(imageRect.width() * scaleX),
    static_cast<int>(imageRect.height() * scaleY));

  return QRect(topLeft, scaledSize);
}

QRect ImageViewWidget::imageCircleToWidgetRect(const ImageCircle& circle) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(drawRect.width()) / m_image.width();
  const double scaleY = static_cast<double>(drawRect.height()) / m_image.height();
  const double scale = (scaleX + scaleY) * 0.5;
  const QPoint center(
    drawRect.x() + static_cast<int>(circle.center.x() * scaleX),
    drawRect.y() + static_cast<int>(circle.center.y() * scaleY));
  const int radius = static_cast<int>(circle.radius * scale);

  return QRect(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
}

QPoint ImageViewWidget::imagePointToWidget(const QPoint& imagePoint) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(drawRect.width()) / m_image.width();
  const double scaleY = static_cast<double>(drawRect.height()) / m_image.height();
  return QPoint(
    drawRect.x() + static_cast<int>(imagePoint.x() * scaleX),
    drawRect.y() + static_cast<int>(imagePoint.y() * scaleY));
}

QPointF ImageViewWidget::imagePointToWidget(const QPointF& imagePoint) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(drawRect.width()) / m_image.width();
  const double scaleY = static_cast<double>(drawRect.height()) / m_image.height();
  return QPointF(
    drawRect.x() + imagePoint.x() * scaleX,
    drawRect.y() + imagePoint.y() * scaleY);
}

QPointF ImageViewWidget::widgetToImageF(const QPoint& widgetPoint) const
{
  const QRect drawRect = imageDrawRect();

  if (drawRect.isEmpty() || m_image.isNull())
  {
    return {};
  }

  const double scaleX = static_cast<double>(m_image.width()) / drawRect.width();
  const double scaleY = static_cast<double>(m_image.height()) / drawRect.height();
  return QPointF(
    qBound(0.0, (widgetPoint.x() - drawRect.x()) * scaleX, static_cast<double>(m_image.width() - 1)),
    qBound(0.0, (widgetPoint.y() - drawRect.y()) * scaleY, static_cast<double>(m_image.height() - 1)));
}

QVector<QPointF> ImageViewWidget::geometryAreaCorners() const
{
  const double halfWidth = m_geometryArea.size.width() * 0.5;
  const double halfHeight = m_geometryArea.size.height() * 0.5;
  constexpr double pi = 3.14159265358979323846;
  const double angle = m_geometryArea.angleDegrees * pi / 180.0;
  const QPointF axisX(std::cos(angle), std::sin(angle));
  const QPointF axisY(-std::sin(angle), std::cos(angle));

  return {
    m_geometryArea.center - axisX * halfWidth - axisY * halfHeight,
    m_geometryArea.center + axisX * halfWidth - axisY * halfHeight,
    m_geometryArea.center + axisX * halfWidth + axisY * halfHeight,
    m_geometryArea.center - axisX * halfWidth + axisY * halfHeight
  };
}

QVector<QPointF> ImageViewWidget::geometryAreaWidgetCorners() const
{
  QVector<QPointF> result;
  for (const QPointF& corner : geometryAreaCorners())
  {
    result.append(imagePointToWidget(corner));
  }
  return result;
}

QPointF ImageViewWidget::geometryAreaWidgetRotateHandle() const
{
  const QVector<QPointF> corners = geometryAreaWidgetCorners();
  if (corners.size() < 2)
  {
    return {};
  }

  const QPointF topCenter = (corners[0] + corners[1]) * 0.5;
  const QPointF center = imagePointToWidget(m_geometryArea.center);
  const QPointF direction = topCenter - center;
  const double length = std::hypot(direction.x(), direction.y());
  if (length <= 0.001)
  {
    return topCenter;
  }

  return topCenter + direction / length * 28.0;
}

ImageViewWidget::GeometryAreaHandle ImageViewWidget::geometryAreaHandleAt(const QPoint& widgetPoint) const
{
  if (!m_hasGeometryArea)
  {
    return GeometryAreaHandle::None;
  }

  const QVector<QPointF> corners = geometryAreaWidgetCorners();
  const QVector<GeometryAreaHandle> handles = {
    GeometryAreaHandle::TopLeft,
    GeometryAreaHandle::TopRight,
    GeometryAreaHandle::BottomRight,
    GeometryAreaHandle::BottomLeft
  };

  for (int i = 0; i < corners.size() && i < handles.size(); ++i)
  {
    if (QRectF(corners[i] - QPointF(7, 7), QSizeF(14, 14)).contains(widgetPoint))
    {
      return handles[i];
    }
  }

  if (QRectF(geometryAreaWidgetRotateHandle() - QPointF(8, 8), QSizeF(16, 16)).contains(widgetPoint))
  {
    return GeometryAreaHandle::Rotate;
  }

  return geometryAreaContains(widgetPoint) ? GeometryAreaHandle::Move : GeometryAreaHandle::None;
}

bool ImageViewWidget::geometryAreaContains(const QPoint& widgetPoint) const
{
  QPolygonF polygon;
  for (const QPointF& corner : geometryAreaWidgetCorners())
  {
    polygon << corner;
  }
  return polygon.containsPoint(widgetPoint, Qt::OddEvenFill);
}

ImageRotatedRect ImageViewWidget::normalizedGeometryArea(const ImageRotatedRect& area) const
{
  ImageRotatedRect result = area;
  result.size.setWidth(std::max(2.0, result.size.width()));
  result.size.setHeight(std::max(2.0, result.size.height()));
  return result;
}

int ImageViewWidget::exclusionRectAt(const QPoint& widgetPoint) const
{
  for (int i = m_exclusionRects.size() - 1; i >= 0; --i)
  {
    const QRect widgetRect = imageRectToWidgetRect(m_exclusionRects[i]).adjusted(-5, -5, 5, 5);

    if (widgetRect.contains(widgetPoint))
    {
      return i;
    }
  }

  return -1;
}

int ImageViewWidget::exclusionHandleAt(const QPoint& widgetPoint, ExclusionHandle& handle) const
{
  handle = ExclusionHandle::None;

  for (int i = m_exclusionRects.size() - 1; i >= 0; --i)
  {
    const QRect widgetRect = imageRectToWidgetRect(m_exclusionRects[i]);
    const QVector<QPair<ExclusionHandle, QPoint>> handles = {
      {ExclusionHandle::TopLeft, widgetRect.topLeft()},
      {ExclusionHandle::TopRight, widgetRect.topRight()},
      {ExclusionHandle::BottomLeft, widgetRect.bottomLeft()},
      {ExclusionHandle::BottomRight, widgetRect.bottomRight()}
    };

    for (const auto& item : handles)
    {
      if (QRect(item.second - QPoint(6, 6), QSize(12, 12)).contains(widgetPoint))
      {
        handle = item.first;
        return i;
      }
    }
  }

  return -1;
}

int ImageViewWidget::geometryOverlayPointAt(const QPoint& widgetPoint) const
{
  for (int i = m_geometryOverlay.points.size() - 1; i >= 0; --i)
  {
    const QPointF widgetHandle = imagePointToWidget(m_geometryOverlay.points[i].imagePoint);
    if (QRectF(widgetHandle - QPointF(9, 9), QSizeF(18, 18)).contains(widgetPoint))
    {
      return i;
    }
  }

  return -1;
}

QRect ImageViewWidget::clampImageRectToImage(const QRect& imageRect) const
{
  if (m_image.isNull())
  {
    return imageRect.normalized();
  }

  const QRect normalized = imageRect.normalized();
  const int maxX = qMax(0, m_image.width() - normalized.width());
  const int maxY = qMax(0, m_image.height() - normalized.height());
  const int x = qBound(0, normalized.x(), maxX);
  const int y = qBound(0, normalized.y(), maxY);
  return QRect(x, y, normalized.width(), normalized.height());
}
