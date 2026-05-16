#include "ImageViewWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <cmath>

ImageViewWidget::ImageViewWidget(QWidget* parent)
  : QWidget(parent)
{
  setMinimumSize(320, 240);
  setMouseTracking(true);
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
  m_exclusionRects.clear();
  m_circles.clear();
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

void ImageViewWidget::setRoiDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Roi : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setExclusionDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Exclusion : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setOuterCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::OuterCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setInnerCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::InnerCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setThreePointCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::ThreePointCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
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

void ImageViewWidget::setCircleChangedHandler(std::function<void(bool, const ImageCircle&)> handler)
{
  m_circleChangedHandler = std::move(handler);
}

void ImageViewWidget::setThreePointCircleHandler(std::function<void(const QVector<QPoint>&)> handler)
{
  m_threePointCircleHandler = std::move(handler);
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

  for (const QRect& rect : m_exclusionRects)
  {
    painter.drawRect(imageRectToWidgetRect(rect));
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

  if (m_dragging)
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
  if (!m_dragging)
  {
    QWidget::mouseMoveEvent(event);
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
