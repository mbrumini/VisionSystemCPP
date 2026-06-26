#include "ImageViewWidget.h"

#include <QPainter>
#include <QPolygon>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace
{
void drawArrowHead(QPainter& painter, const QPointF& tip, const QPointF& from, const QColor& color)
{
  const QPointF direction = tip - from;
  const double length = std::hypot(direction.x(), direction.y());
  if (length < 0.001)
  {
    return;
  }

  const QPointF unit(direction.x() / length, direction.y() / length);
  const QPointF normal(-unit.y(), unit.x());
  constexpr double arrowLength = 13.0;
  constexpr double arrowHalfWidth = 5.5;
  QPolygonF arrow;
  arrow << tip
        << tip - unit * arrowLength + normal * arrowHalfWidth
        << tip - unit * arrowLength - normal * arrowHalfWidth;
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawPolygon(arrow);
}

void drawMeasurementLabel(QPainter& painter,
                          const QPointF& point,
                          const QString& label,
                          const QColor& color)
{
  const QFont previousFont = painter.font();
  QFont labelFont = previousFont;
  if (labelFont.pointSize() > 0)
  {
    labelFont.setPointSize(labelFont.pointSize() + 4);
  }
  else
  {
    labelFont.setPixelSize(std::max(18, labelFont.pixelSize() + 5));
  }
  labelFont.setBold(true);
  painter.setFont(labelFont);

  painter.setPen(QPen(QColor("#101418"), 7));
  painter.drawText(point, label);
  painter.setPen(QPen(color, 2));
  painter.drawText(point, label);
  painter.setFont(previousFont);
}

void drawAngleOverlay(QPainter& painter,
                      const QPointF& center,
                      const QPointF& armA,
                      const QPointF& armB,
                      const QString& label,
                      const QColor& color,
                      int width,
                      const QColor& labelColor,
                      bool hasCustomLabelPoint = false,
                      const QPointF& customLabelPoint = {})
{
  const QPointF vectorA = armA - center;
  const QPointF vectorB = armB - center;
  const double lengthA = std::hypot(vectorA.x(), vectorA.y());
  const double lengthB = std::hypot(vectorB.x(), vectorB.y());
  if (lengthA < 0.001 || lengthB < 0.001)
  {
    return;
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

  QPolygonF arc;
  constexpr int steps = 24;
  for (int i = 0; i <= steps; ++i)
  {
    const double t = static_cast<double>(i) / steps;
    const double angle = startAngle + delta * t;
    arc << center + QPointF(std::cos(angle), std::sin(angle)) * radius;
  }

  painter.setPen(QPen(color, width));
  painter.setBrush(Qt::NoBrush);
  painter.drawPolyline(arc);
  if (arc.size() >= 2)
  {
    drawArrowHead(painter, arc.last(), arc[arc.size() - 2], color);
  }

  const double labelAngle = startAngle + delta * 0.5;
  const QPointF labelPoint = hasCustomLabelPoint
    ? customLabelPoint
    : center + QPointF(std::cos(labelAngle), std::sin(labelAngle)) * (radius + 14.0);
  drawMeasurementLabel(painter, labelPoint, label, labelColor);
}
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
    const QRect widgetRoi = imageRectToWidgetRect(m_roi);
    painter.drawRect(widgetRoi);

    if (m_drawingMode == DrawingMode::Roi)
    {
      QPen selectedPen(QColor("#ffffff"));
      selectedPen.setWidth(1);
      selectedPen.setStyle(Qt::DashLine);
      painter.setPen(selectedPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(widgetRoi.adjusted(-3, -3, 3, 3));
      painter.setBrush(QColor("#ffffff"));
      const QVector<QPoint> handles = {
        widgetRoi.topLeft(),
        widgetRoi.topRight(),
        widgetRoi.bottomLeft(),
        widgetRoi.bottomRight()
      };
      for (const QPoint& handle : handles)
      {
        painter.drawRect(QRect(handle - QPoint(4, 4), QSize(8, 8)));
      }
      painter.setPen(roiPen);
      painter.setBrush(Qt::NoBrush);
    }
  }

  QPen polygonPen(QColor("#00d2ff"));
  polygonPen.setWidth(2);
  if (m_searchPolygon.size() >= 2)
  {
    QPolygon widgetPolygon;
    for (const QPoint& point : m_searchPolygon)
    {
      widgetPolygon << imagePointToWidget(point);
    }

    painter.setPen(polygonPen);
    painter.setBrush(m_searchPolygon.size() >= 3 ? QColor(0, 210, 255, 35) : Qt::NoBrush);
    if (m_searchPolygon.size() >= 3)
    {
      painter.drawPolygon(widgetPolygon);
    }
    else
    {
      painter.drawPolyline(widgetPolygon);
    }

    painter.setBrush(QColor("#ffffff"));
    for (int i = 0; i < widgetPolygon.size(); ++i)
    {
      const QPoint point = widgetPolygon[i];
      painter.drawRect(QRect(point - QPoint(4, 4), QSize(8, 8)));
      painter.drawText(point + QPoint(8, -8), QString::number(i + 1));
    }
  }

  if (!m_pendingPolygon.isEmpty())
  {
    QPolygon pendingPolygon;
    for (const QPoint& point : m_pendingPolygon)
    {
      pendingPolygon << imagePointToWidget(point);
    }

    painter.setPen(polygonPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(pendingPolygon);
    painter.setBrush(QColor("#00d2ff"));
    for (int i = 0; i < pendingPolygon.size(); ++i)
    {
      const QPoint point = pendingPolygon[i];
      painter.drawEllipse(point, 4, 4);
      painter.drawText(point + QPoint(8, -8), QString::number(i + 1));
    }
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
    const bool guideCircle = m_circleBandEditing &&
      m_circles.size() == 3 &&
      i == 1;
    painter.setPen(guideCircle ? QPen(QColor("#00d2ff"), 2) : (i == 0 ? outerCirclePen : innerCirclePen));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(imageCircleToWidgetRect(m_circles[i]));
  }

  if (m_hasDetectedCircle)
  {
    QPen detectedCirclePen(QColor("#00d2ff"));
    detectedCirclePen.setWidth(5);
    painter.setPen(detectedCirclePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(imageCircleToWidgetRect(m_detectedCircle));
  }

  if (m_circleBandEditing && !m_circles.isEmpty())
  {
    painter.setPen(QPen(QColor("#ffffff"), 3));
    painter.setBrush(QColor("#ff2bd6"));
    const QPoint center = imagePointToWidget(m_circles.first().center);
    painter.drawRect(QRect(center - QPoint(9, 9), QSize(18, 18)));
    painter.drawLine(center - QPoint(14, 0), center + QPoint(14, 0));
    painter.drawLine(center - QPoint(0, 14), center + QPoint(0, 14));

    for (int i = 0; i < m_circles.size(); ++i)
    {
      if (m_circles.size() == 3 && i == 1)
      {
        continue;
      }
      const ImageCircle& circle = m_circles[i];
      const int offset = qRound(circle.radius * 0.70710678118);
      const bool innerCircle = i == m_circles.size() - 1;
      const QPoint handle = imagePointToWidget(
        circle.center + QPoint(offset, innerCircle ? offset : -offset));
      painter.setBrush(QColor("#ff2bd6"));
      painter.drawRect(QRect(handle - QPoint(8, 8), QSize(16, 16)));
    }
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

    for (const GeometryOverlayDimension& dimension : m_geometryOverlay.dimensions)
    {
      const QPointF start = imagePointToWidget(dimension.imageStart);
      const QPointF end = imagePointToWidget(dimension.imageEnd);
      painter.setPen(QPen(dimension.color, dimension.width));
      painter.setBrush(Qt::NoBrush);
      painter.drawLine(start, end);
      drawArrowHead(painter, start, end, dimension.color);
      drawArrowHead(painter, end, start, dimension.color);

      const QPointF labelPoint = dimension.hasLabelPoint
        ? imagePointToWidget(dimension.labelPoint)
        : (start + end) * 0.5 + QPointF(10, -10);
      drawMeasurementLabel(painter, labelPoint, dimension.label, dimension.labelColor);
    }

    for (const GeometryOverlayAngle& angle : m_geometryOverlay.angles)
    {
      const QPointF center = imagePointToWidget(angle.imageCenter);
      const QPointF armA = imagePointToWidget(angle.imageArmA);
      const QPointF armB = imagePointToWidget(angle.imageArmB);
      const QPointF labelPoint = angle.hasLabelPoint
        ? imagePointToWidget(angle.labelPoint)
        : QPointF();
      drawAngleOverlay(painter,
                       center,
                       armA,
                       armB,
                       angle.label,
                       angle.color,
                       angle.width,
                       angle.labelColor,
                       angle.hasLabelPoint,
                       labelPoint);
    }

    for (const GeometryOverlayPoint& point : m_geometryOverlay.points)
    {
      const QPointF widgetPoint = imagePointToWidget(point.imagePoint);
      painter.setPen(QPen(QColor("#ffffff"), 2));
      painter.setBrush(point.color);
      painter.drawEllipse(widgetPoint, point.radius, point.radius);
      if (!point.label.isEmpty())
      {
        painter.drawText(widgetPoint + QPointF(point.radius + 3.0, -point.radius - 3.0), point.label);
      }
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

  if (m_dragging &&
      !m_movingRoi &&
      !m_resizingRoi &&
      !m_movingExclusion &&
      !m_movingGeometryOverlayPoint &&
      !m_movingCircleBandCenter &&
      m_selectedCircleBandRadius < 0)
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

