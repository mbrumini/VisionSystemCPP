#include "ImageViewWidget.h"

#include <QMouseEvent>
#include <QPainter>

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

void ImageViewWidget::setRoiDrawingEnabled(bool enabled)
{
  m_roiDrawingEnabled = enabled;
  m_dragging = false;
  update();
}

void ImageViewWidget::setRoiChangedHandler(std::function<void(const QRect&)> handler)
{
  m_roiChangedHandler = std::move(handler);
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

  if (m_dragging)
  {
    painter.drawRect(QRect(m_dragStart, m_dragEnd).normalized());
  }
}

void ImageViewWidget::mousePressEvent(QMouseEvent* event)
{
  if (!m_roiDrawingEnabled || event->button() != Qt::LeftButton || m_image.isNull())
  {
    QWidget::mousePressEvent(event);
    return;
  }

  if (!imageDrawRect().contains(event->pos()))
  {
    return;
  }

  m_dragging = true;
  m_dragStart = event->pos();
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

  const QRect imageRoi = widgetRectToImageRect(QRect(m_dragStart, m_dragEnd).normalized());

  if (imageRoi.width() > 2 && imageRoi.height() > 2)
  {
    setRoi(imageRoi);

    if (m_roiChangedHandler)
    {
      m_roiChangedHandler(m_roi);
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
