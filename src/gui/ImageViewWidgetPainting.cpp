#include "ImageViewWidget.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

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

    if (m_drawingMode == DrawingMode::ThreePointArc && m_threePointCirclePoints.size() >= 2)
    {
      painter.setBrush(Qt::NoBrush);
      painter.drawLine(imagePointToWidget(m_threePointCirclePoints[0]), imagePointToWidget(m_threePointCirclePoints[1]));
      painter.setBrush(QColor("#00d2ff"));
    }

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

