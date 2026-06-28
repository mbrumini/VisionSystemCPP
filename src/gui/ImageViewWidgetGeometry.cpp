#include "ImageViewWidget.h"

#include <QFontMetrics>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

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

bool ImageViewWidget::roiContains(const QPoint& widgetPoint) const
{
  if (!m_hasRoi)
  {
    return false;
  }

  return imageRectToWidgetRect(m_roi).adjusted(-5, -5, 5, 5).contains(widgetPoint);
}

bool ImageViewWidget::roiHandleAt(const QPoint& widgetPoint, ExclusionHandle& handle) const
{
  handle = ExclusionHandle::None;

  if (!m_hasRoi)
  {
    return false;
  }

  const QRect widgetRect = imageRectToWidgetRect(m_roi);
  const QVector<QPair<ExclusionHandle, QPoint>> handles = {
    {ExclusionHandle::TopLeft, widgetRect.topLeft()},
    {ExclusionHandle::TopRight, widgetRect.topRight()},
    {ExclusionHandle::BottomLeft, widgetRect.bottomLeft()},
    {ExclusionHandle::BottomRight, widgetRect.bottomRight()}
  };

  for (const auto& item : handles)
  {
    if (QRect(item.second - QPoint(7, 7), QSize(14, 14)).contains(widgetPoint))
    {
      handle = item.first;
      return true;
    }
  }

  return false;
}

int ImageViewWidget::geometryOverlayPointAt(const QPoint& widgetPoint) const
{
  for (int i = m_geometryOverlay.points.size() - 1; i >= 0; --i)
  {
    if (!m_geometryOverlay.points[i].editable)
    {
      continue;
    }

    const QPointF widgetHandle = imagePointToWidget(m_geometryOverlay.points[i].imagePoint);
    if (QRectF(widgetHandle - QPointF(9, 9), QSizeF(18, 18)).contains(widgetPoint))
    {
      return i;
    }
  }

  return -1;
}

QPointF ImageViewWidget::geometryOverlayDimensionLabelPoint(const GeometryOverlayDimension& dimension) const
{
  if (dimension.hasLabelPoint)
  {
    return imagePointToWidget(dimension.labelPoint);
  }

  return (imagePointToWidget(dimension.imageStart) + imagePointToWidget(dimension.imageEnd)) * 0.5 + QPointF(10, -10);
}

namespace
{
QFont measurementLabelFont(const QFont& baseFont)
{
  QFont labelFont = baseFont;
  if (labelFont.pointSize() > 0)
  {
    labelFont.setPointSize(labelFont.pointSize() + 4);
  }
  else
  {
    labelFont.setPixelSize(std::max(18, labelFont.pixelSize() + 5));
  }
  labelFont.setBold(true);
  return labelFont;
}

QRectF measurementLabelHitRect(const QFontMetrics& metrics, const QString& label, const QPointF& labelPoint)
{
  QRectF labelRect = metrics.boundingRect(label);
  labelRect.moveTopLeft(labelPoint + QPointF(0, -labelRect.height()));
  return labelRect.adjusted(-12, -10, 12, 10);
}

QPointF defaultAngleLabelWidgetPoint(const QPointF& center, const QPointF& armA, const QPointF& armB)
{
  const QPointF vectorA = armA - center;
  const QPointF vectorB = armB - center;
  const double lengthA = std::hypot(vectorA.x(), vectorA.y());
  const double lengthB = std::hypot(vectorB.x(), vectorB.y());
  if (lengthA < 0.001 || lengthB < 0.001)
  {
    return center + QPointF(10, -10);
  }

  const double radius = std::max(18.0, std::min({48.0, lengthA * 0.35, lengthB * 0.35}));
  const double startAngle = std::atan2(vectorA.y(), vectorA.x());
  double delta = std::atan2(vectorB.y(), vectorB.x()) - startAngle;
  while (delta > 3.14159265358979323846)
  {
    delta -= 2.0 * 3.14159265358979323846;
  }
  while (delta < -3.14159265358979323846)
  {
    delta += 2.0 * 3.14159265358979323846;
  }

  const double labelAngle = startAngle + delta * 0.5;
  return center + QPointF(std::cos(labelAngle), std::sin(labelAngle)) * (radius + 14.0);
}
}

QPointF ImageViewWidget::geometryOverlayAngleLabelPoint(const GeometryOverlayAngle& angle) const
{
  const QPointF center = imagePointToWidget(angle.imageCenter);
  const QPointF armA = imagePointToWidget(angle.imageArmA);
  const QPointF armB = imagePointToWidget(angle.imageArmB);
  if (angle.hasLabelPoint)
  {
    return imagePointToWidget(angle.labelPoint);
  }

  return defaultAngleLabelWidgetPoint(center, armA, armB);
}

int ImageViewWidget::geometryOverlayAngleLabelAt(const QPoint& widgetPoint) const
{
  const QFontMetrics metrics(measurementLabelFont(font()));

  for (int i = m_geometryOverlay.angles.size() - 1; i >= 0; --i)
  {
    const GeometryOverlayAngle& angle = m_geometryOverlay.angles[i];
    if (angle.id.isEmpty() || angle.label.isEmpty())
    {
      continue;
    }

    const QPointF labelPoint = geometryOverlayAngleLabelPoint(angle);
    if (measurementLabelHitRect(metrics, angle.label, labelPoint).contains(widgetPoint))
    {
      return i;
    }
  }

  return -1;
}

int ImageViewWidget::geometryOverlayDimensionLabelAt(const QPoint& widgetPoint) const
{
  const QFontMetrics metrics(measurementLabelFont(font()));

  for (int i = m_geometryOverlay.dimensions.size() - 1; i >= 0; --i)
  {
    const GeometryOverlayDimension& dimension = m_geometryOverlay.dimensions[i];
    if (dimension.id.isEmpty() || dimension.label.isEmpty())
    {
      continue;
    }

    const QPointF labelPoint = geometryOverlayDimensionLabelPoint(dimension);
    if (measurementLabelHitRect(metrics, dimension.label, labelPoint).contains(widgetPoint))
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
