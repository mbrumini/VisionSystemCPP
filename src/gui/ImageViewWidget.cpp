#include "ImageViewWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
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

void ImageViewWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.fillRect(rect(), QColor("#101418"));

  if (m_image.isNull())
  {
    painter.setPen(QColor("#9aa4ad"));
    painter.drawText(rect(), Qt::AlignCenter, "NO IMAGE");
    return;
  }

  const QRect drawRect = imageDrawRect();
  painter.drawPixmap(drawRect, m_image);

  QPen roiPen(QColor("#f5d547"));
  roiPen.setWidth(2);
  painter.setPen(roiPen);
  painter.setBrush(Qt::NoBrush);

  if (m_hasRoi)
  {
    painter.drawRect(imageRectToWidgetRect(m_roi));
  }

  QPen exclusionPen(QColor("#ff3b30"));
  exclusionPen.setWidth(2);
  painter.setPen(exclusionPen);
  painter.setBrush(QColor(255, 59, 48, 55));

  for (int i = 0; i < m_exclusionRects.size(); ++i)
  {
    const QRect widgetRect = imageRectToWidgetRect(m_exclusionRects[i]);
    painter.drawRect(widgetRect);

    if (i == m_selectedExclusionIndex)
    {
      QPen selectedPen(QColor("#ffffff"));
      selectedPen.setWidth(1);
      selectedPen.setStyle(Qt::DashLine);
      painter.setPen(selectedPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(widgetRect.adjusted(-3, -3, 3, 3));
      painter.setBrush(QColor("#ffffff"));
      const QVector<QPoint> handles = {
        widgetRect.topLeft(),
        widgetRect.topRight(),
        widgetRect.bottomLeft(),
        widgetRect.bottomRight()
      };
      for (const QPoint& handle : handles)
      {
        painter.drawRect(QRect(handle - QPoint(3, 3), QSize(6, 6)));
      }
      painter.setPen(exclusionPen);
      painter.setBrush(QColor(255, 59, 48, 55));
    }
  }

  QPen outerCirclePen(QColor("#35c46a"));
  outerCirclePen.setWidth(2);
  QPen innerCirclePen(QColor("#ff9f0a"));
  innerCirclePen.setWidth(2);

  for (int i = 0; i < m_circles.size(); ++i)
  {
    painter.setPen(i == 0 ? outerCirclePen : innerCirclePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(imageCircleToWidgetRect(m_circles[i]));
  }

  QPen geometryAreaPen(QColor("#00d2ff"));
  geometryAreaPen.setWidth(2);
  QPen geometryLinePen(QColor("#ff4fd8"));
  geometryLinePen.setWidth(3);

  if (m_hasGeometryArea)
  {
    const QVector<QPointF> corners = geometryAreaWidgetCorners();
    QPolygonF polygon;
    for (const QPointF& corner : corners)
    {
      polygon << corner;
    }

    painter.setPen(geometryAreaPen);
    painter.setBrush(QColor(0, 210, 255, 35));
    painter.drawPolygon(polygon);

    painter.setBrush(QColor("#00d2ff"));
    for (const QPointF& corner : corners)
    {
      painter.drawRect(QRectF(corner - QPointF(4, 4), QSizeF(8, 8)));
    }

    const QPointF topCenter = (corners[0] + corners[1]) * 0.5;
    const QPointF rotateHandle = geometryAreaWidgetRotateHandle();
    painter.drawLine(topCenter, rotateHandle);
    painter.setBrush(QColor("#ffffff"));
    painter.drawEllipse(rotateHandle, 5, 5);
  }

  if (!m_geometryLines.isEmpty())
  {
    painter.setPen(geometryLinePen);
    for (const ImageLine& line : m_geometryLines)
    {
      painter.drawLine(imagePointToWidget(line.start), imagePointToWidget(line.end));
    }
  }

  if (!m_geometryPoints.isEmpty())
  {
    painter.setPen(QPen(QColor("#ffffff"), 2));
    painter.setBrush(QColor("#ff4fd8"));
    for (int i = 0; i < m_geometryPoints.size(); ++i)
    {
      const QPointF widgetPoint = imagePointToWidget(m_geometryPoints[i]);
      painter.drawEllipse(widgetPoint, 6, 6);
      painter.drawText(widgetPoint + QPointF(9, -9), QString::number(i + 1));
    }
  }

  if (!m_geometryOverlay.empty())
  {
    for (const GeometryOverlayBand& band : m_geometryOverlay.bands)
    {
      const double angle = band.angleDegrees * 3.14159265358979323846 / 180.0;
      const QPointF axisX(std::cos(angle), std::sin(angle));
      const QPointF axisY(-std::sin(angle), std::cos(angle));
      const double halfWidth = band.imageSize.width() * 0.5;
      const double halfHeight = band.imageSize.height() * 0.5;
      const QVector<QPointF> imageCorners = {
        band.imageCenter - axisX * halfWidth - axisY * halfHeight,
        band.imageCenter + axisX * halfWidth - axisY * halfHeight,
        band.imageCenter + axisX * halfWidth + axisY * halfHeight,
        band.imageCenter - axisX * halfWidth + axisY * halfHeight
      };

      QPolygonF polygon;
      for (const QPointF& corner : imageCorners)
      {
        polygon << imagePointToWidget(corner);
      }

      painter.setPen(QPen(band.outlineColor, 2));
      painter.setBrush(band.fillColor);
      painter.drawPolygon(polygon);
    }

    for (const GeometryOverlayLine& line : m_geometryOverlay.lines)
    {
      painter.setPen(QPen(line.color, line.width));
      painter.drawLine(imagePointToWidget(line.imageStart), imagePointToWidget(line.imageEnd));
    }

    for (const GeometryOverlayPoint& point : m_geometryOverlay.points)
    {
      const QPointF widgetPoint = imagePointToWidget(point.imagePoint);
      painter.setPen(QPen(QColor("#ffffff"), 2));
      painter.setBrush(point.color);
      painter.drawEllipse(widgetPoint, 6, 6);
      painter.drawText(widgetPoint + QPointF(9, -9), point.label);
    }
  }

  if (!m_threePointCirclePoints.isEmpty())
  {
    QPen pointPen(QColor("#00d2ff"));
    pointPen.setWidth(2);
    painter.setPen(pointPen);
    painter.setBrush(QColor("#00d2ff"));

    for (const QPoint& imagePoint : m_threePointCirclePoints)
    {
      const QPoint widgetPoint = imagePointToWidget(imagePoint);
      painter.drawEllipse(widgetPoint, 4, 4);
    }
  }

  if (!m_twoPointLinePoints.isEmpty())
  {
    QPen pointPen(QColor("#ff4fd8"));
    pointPen.setWidth(2);
    painter.setPen(pointPen);
    painter.setBrush(QColor("#ff4fd8"));

    for (const QPoint& imagePoint : m_twoPointLinePoints)
    {
      const QPoint widgetPoint = imagePointToWidget(imagePoint);
      painter.drawEllipse(widgetPoint, 4, 4);
    }

    if (m_twoPointLinePoints.size() == 1)
    {
      painter.drawText(imagePointToWidget(m_twoPointLinePoints.first()) + QPoint(8, -8), "1");
    }
  }

  if (m_dragging && !m_movingExclusion && !m_movingGeometryOverlayPoint)
  {
    if (m_drawingMode == DrawingMode::OuterCircle || m_drawingMode == DrawingMode::InnerCircle)
    {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(m_drawingMode == DrawingMode::OuterCircle ? outerCirclePen : innerCirclePen);
      const int radius = static_cast<int>(std::hypot(m_dragEnd.x() - m_dragStart.x(), m_dragEnd.y() - m_dragStart.y()));
      painter.drawEllipse(m_dragStart, radius, radius);
    }
    else
    {
      painter.setBrush(m_drawingMode == DrawingMode::Exclusion ? QColor(255, 59, 48, 55) : Qt::NoBrush);
      painter.setPen(m_drawingMode == DrawingMode::Exclusion ? exclusionPen : roiPen);
      painter.drawRect(QRect(m_dragStart, m_dragEnd).normalized());
    }
  }
}

void ImageViewWidget::keyPressEvent(QKeyEvent* event)
{
  if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
      m_selectedExclusionIndex >= 0 &&
      m_selectedExclusionIndex < m_exclusionRects.size())
  {
    m_exclusionRects.removeAt(m_selectedExclusionIndex);
    m_selectedExclusionIndex = -1;
    m_movingExclusion = false;
    m_resizingExclusion = false;
    m_activeExclusionHandle = ExclusionHandle::None;
    m_dragging = false;

    if (m_exclusionRectsChangedHandler)
    {
      m_exclusionRectsChangedHandler(m_exclusionRects);
    }

    update();
    return;
  }

  QWidget::keyPressEvent(event);
}

void ImageViewWidget::mousePressEvent(QMouseEvent* event)
{
  if (m_drawingMode == DrawingMode::None || event->button() != Qt::LeftButton || m_image.isNull())
  {
    QWidget::mousePressEvent(event);
    return;
  }

  if (!imageDrawRect().contains(event->pos()))
  {
    return;
  }

  setFocus();

  if (m_drawingMode == DrawingMode::Exclusion)
  {
    ExclusionHandle handle = ExclusionHandle::None;
    const int handleIndex = exclusionHandleAt(event->pos(), handle);

    if (handleIndex >= 0)
    {
      m_selectedExclusionIndex = handleIndex;
      m_activeExclusionHandle = handle;
      m_resizingExclusion = true;
      m_dragging = true;
      m_moveStartImageRect = m_exclusionRects[handleIndex];
      update();
      return;
    }

    const int hitIndex = exclusionRectAt(event->pos());
    if (hitIndex >= 0)
    {
      m_selectedExclusionIndex = hitIndex;
      m_movingExclusion = true;
      m_dragging = true;
      m_moveStartImagePoint = widgetToImage(event->pos());
      m_moveStartImageRect = m_exclusionRects[hitIndex];
      update();
      return;
    }

    m_selectedExclusionIndex = -1;
    m_activeExclusionHandle = ExclusionHandle::None;
  }

  if (m_drawingMode == DrawingMode::GeometryArea && m_hasGeometryArea)
  {
    const GeometryAreaHandle handle = geometryAreaHandleAt(event->pos());
    if (handle != GeometryAreaHandle::None)
    {
      m_activeGeometryAreaHandle = handle;
      m_moveStartGeometryArea = m_geometryArea;
      m_moveStartImagePoint = widgetToImage(event->pos());
      m_dragging = true;
      update();
      return;
    }
  }

  if (m_drawingMode == DrawingMode::GeometryPointPick)
  {
    if (m_geometryPointPickedHandler)
    {
      m_geometryPointPickedHandler(widgetToImageF(event->pos()));
    }
    update();
    return;
  }

  if (m_drawingMode == DrawingMode::GeometryOverlayPointEdit)
  {
    const int pointIndex = geometryOverlayPointAt(event->pos());
    if (pointIndex >= 0)
    {
      m_selectedGeometryOverlayPointIndex = pointIndex;
      m_movingGeometryOverlayPoint = true;
      m_dragging = true;
      setCursor(Qt::ClosedHandCursor);
      update();
    }
    return;
  }

  if (m_drawingMode == DrawingMode::ThreePointCircle)
  {
    m_threePointCirclePoints.append(widgetToImage(event->pos()));

    if (m_threePointCirclePoints.size() >= 3)
    {
      const QVector<QPoint> points = m_threePointCirclePoints;
      m_threePointCirclePoints.clear();

      if (m_threePointCircleHandler)
      {
        m_threePointCircleHandler(points);
      }
    }

    update();
    return;
  }

  if (m_drawingMode == DrawingMode::TwoPointLine)
  {
    m_twoPointLinePoints.append(widgetToImage(event->pos()));

    if (m_twoPointLinePoints.size() >= 2)
    {
      const QVector<QPoint> points = m_twoPointLinePoints;
      m_twoPointLinePoints.clear();

      if (m_twoPointLineHandler)
      {
        m_twoPointLineHandler(points);
      }
    }

    update();
    return;
  }

  m_dragging = true;
  if (m_drawingMode == DrawingMode::InnerCircle && !m_circles.isEmpty())
  {
    m_dragStart = imagePointToWidget(m_circles[0].center);
  }
  else
  {
    m_dragStart = event->pos();
  }
  m_dragEnd = event->pos();
  update();
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (m_drawingMode == DrawingMode::GeometryPointPick)
  {
    if (m_geometryPointMovedHandler && imageDrawRect().contains(event->pos()))
    {
      m_geometryPointMovedHandler(widgetToImageF(event->pos()));
    }
    QWidget::mouseMoveEvent(event);
    return;
  }

  if (!m_dragging)
  {
    QWidget::mouseMoveEvent(event);
    return;
  }

  if (m_movingGeometryOverlayPoint &&
      m_selectedGeometryOverlayPointIndex >= 0 &&
      m_selectedGeometryOverlayPointIndex < m_geometryOverlay.points.size())
  {
    if (m_geometryOverlayPointMovedHandler && imageDrawRect().contains(event->pos()))
    {
      m_geometryOverlayPointMovedHandler(m_selectedGeometryOverlayPointIndex, widgetToImageF(event->pos()));
    }
    update();
    return;
  }

  if (m_movingExclusion &&
      m_selectedExclusionIndex >= 0 &&
      m_selectedExclusionIndex < m_exclusionRects.size())
  {
    const QPoint currentImagePoint = widgetToImage(event->pos());
    const QPoint delta = currentImagePoint - m_moveStartImagePoint;
    QRect moved = m_moveStartImageRect.translated(delta);
    m_exclusionRects[m_selectedExclusionIndex] = clampImageRectToImage(moved);
    update();
    return;
  }

  if (m_resizingExclusion &&
      m_selectedExclusionIndex >= 0 &&
      m_selectedExclusionIndex < m_exclusionRects.size())
  {
    const QPoint currentImagePoint = widgetToImage(event->pos());
    QRect resized = m_moveStartImageRect;

    if (m_activeExclusionHandle == ExclusionHandle::TopLeft)
    {
      resized.setTopLeft(currentImagePoint);
    }
    else if (m_activeExclusionHandle == ExclusionHandle::TopRight)
    {
      resized.setTopRight(currentImagePoint);
    }
    else if (m_activeExclusionHandle == ExclusionHandle::BottomLeft)
    {
      resized.setBottomLeft(currentImagePoint);
    }
    else if (m_activeExclusionHandle == ExclusionHandle::BottomRight)
    {
      resized.setBottomRight(currentImagePoint);
    }

    resized = clampImageRectToImage(resized);
    if (resized.width() > 2 && resized.height() > 2)
    {
      m_exclusionRects[m_selectedExclusionIndex] = resized;
    }

    update();
    return;
  }

  if (m_drawingMode == DrawingMode::GeometryArea &&
      m_activeGeometryAreaHandle != GeometryAreaHandle::None &&
      m_hasGeometryArea)
  {
    const QPointF currentImagePoint = widgetToImageF(event->pos());
    const QPointF startImagePoint = QPointF(m_moveStartImagePoint);

    if (m_activeGeometryAreaHandle == GeometryAreaHandle::Move)
    {
      const QPointF delta = currentImagePoint - startImagePoint;
      m_geometryArea.center = m_moveStartGeometryArea.center + delta;
    }
    else if (m_activeGeometryAreaHandle == GeometryAreaHandle::Rotate)
    {
      const QPointF delta = currentImagePoint - m_moveStartGeometryArea.center;
      m_geometryArea.angleDegrees = std::atan2(delta.y(), delta.x()) * 180.0 / 3.14159265358979323846 + 90.0;
    }
    else
    {
      const double angle = m_moveStartGeometryArea.angleDegrees * 3.14159265358979323846 / 180.0;
      const QPointF axisX(std::cos(angle), std::sin(angle));
      const QPointF axisY(-std::sin(angle), std::cos(angle));
      const QPointF delta = currentImagePoint - m_moveStartGeometryArea.center;
      const double projectionX = delta.x() * axisX.x() + delta.y() * axisX.y();
      const double projectionY = delta.x() * axisY.x() + delta.y() * axisY.y();
      m_geometryArea.size.setWidth(std::max(4.0, std::abs(projectionX) * 2.0));
      m_geometryArea.size.setHeight(std::max(4.0, std::abs(projectionY) * 2.0));
    }

    update();
    return;
  }

  m_dragEnd = event->pos();
  update();
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent* event)
{
  if (!m_dragging || event->button() != Qt::LeftButton)
  {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  m_dragging = false;
  m_dragEnd = event->pos();

  if (m_movingExclusion || m_resizingExclusion)
  {
    m_movingExclusion = false;
    m_resizingExclusion = false;
    m_activeExclusionHandle = ExclusionHandle::None;

    if (m_exclusionRectsChangedHandler)
    {
      m_exclusionRectsChangedHandler(m_exclusionRects);
    }

    update();
    return;
  }

  if (m_movingGeometryOverlayPoint)
  {
    if (m_geometryOverlayPointMovedHandler && imageDrawRect().contains(event->pos()))
    {
      m_geometryOverlayPointMovedHandler(m_selectedGeometryOverlayPointIndex, widgetToImageF(event->pos()));
    }

    m_movingGeometryOverlayPoint = false;
    m_selectedGeometryOverlayPointIndex = -1;
    setCursor(Qt::OpenHandCursor);
    update();
    return;
  }

  if (m_drawingMode == DrawingMode::GeometryArea &&
      m_activeGeometryAreaHandle != GeometryAreaHandle::None)
  {
    m_activeGeometryAreaHandle = GeometryAreaHandle::None;

    if (m_geometryAreaChangedHandler)
    {
      m_geometryAreaChangedHandler(m_geometryArea);
    }

    update();
    return;
  }

  if (m_drawingMode == DrawingMode::OuterCircle || m_drawingMode == DrawingMode::InnerCircle)
  {
    const bool isOuter = m_drawingMode == DrawingMode::OuterCircle;
    const QPoint center = isOuter || m_circles.isEmpty() ? widgetToImage(m_dragStart) : m_circles[0].center;
    const QPoint edge = widgetToImage(m_dragEnd);
    const int radius = static_cast<int>(std::hypot(edge.x() - center.x(), edge.y() - center.y()));

    if (radius > 2)
    {
      ImageCircle circle{center, radius};

      if (isOuter)
      {
        if (m_circles.isEmpty())
        {
          m_circles.append(circle);
        }
        else
        {
          m_circles[0] = circle;
        }
      }
      else
      {
        while (m_circles.size() < 1)
        {
          m_circles.append(circle);
        }

        if (m_circles.size() == 1)
        {
          m_circles.append(circle);
        }
        else
        {
          m_circles[1] = circle;
        }
      }

      if (m_circleChangedHandler)
      {
        m_circleChangedHandler(isOuter, circle);
      }
    }

    update();
    return;
  }

  const QRect imageRoi = widgetRectToImageRect(QRect(m_dragStart, m_dragEnd).normalized());

  if (imageRoi.width() > 2 && imageRoi.height() > 2)
  {
    if (m_drawingMode == DrawingMode::Roi)
    {
      setRoi(imageRoi);

      if (m_roiChangedHandler)
      {
        m_roiChangedHandler(m_roi);
      }
    }
    else if (m_drawingMode == DrawingMode::Exclusion)
    {
      m_exclusionRects.append(imageRoi.normalized());
      m_selectedExclusionIndex = m_exclusionRects.size() - 1;

      if (m_exclusionRectAddedHandler)
      {
        m_exclusionRectAddedHandler(imageRoi.normalized());
      }
    }
  }

  update();
}

QRect ImageViewWidget::imageDrawRect() const
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
