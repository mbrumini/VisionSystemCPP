#include "ImageViewWidget.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

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

  if (m_movingRoi || m_resizingRoi)
  {
    m_movingRoi = false;
    m_resizingRoi = false;
    m_activeRoiHandle = ExclusionHandle::None;

    if (m_roiChangedHandler)
    {
      m_roiChangedHandler(m_roi);
    }

    update();
    return;
  }

  if (m_movingPolygon || m_movingPolygonVertex)
  {
    m_movingPolygon = false;
    m_movingPolygonVertex = false;
    m_selectedPolygonVertexIndex = -1;

    if (m_polygonChangedHandler && m_searchPolygon.size() >= 3)
    {
      m_polygonChangedHandler(m_searchPolygon);
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

  if (m_movingGeometryOverlayDimensionLabel)
  {
    if (m_geometryOverlayDimensionLabelMovedHandler &&
        m_selectedGeometryOverlayDimensionIndex >= 0 &&
        m_selectedGeometryOverlayDimensionIndex < m_geometryOverlay.dimensions.size() &&
        imageDrawRect().contains(event->pos()))
    {
      const QPointF imagePoint = widgetToImageF(event->pos());
      GeometryOverlayDimension& dimension = m_geometryOverlay.dimensions[m_selectedGeometryOverlayDimensionIndex];
      dimension.labelPoint = imagePoint;
      dimension.hasLabelPoint = true;
      m_geometryOverlayDimensionLabelMovedHandler(dimension.id, imagePoint);
    }

    m_movingGeometryOverlayDimensionLabel = false;
    m_selectedGeometryOverlayDimensionIndex = -1;
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
    update();
    return;
  }

  if (m_movingCircleBandCenter || m_selectedCircleBandRadius >= 0)
  {
    const int changedRadiusIndex = m_movingCircleBandCenter ? -1 : m_selectedCircleBandRadius;
    m_movingCircleBandCenter = false;
    m_selectedCircleBandRadius = -1;
    setCursor(Qt::OpenHandCursor);
    if (m_circleBandChangedHandler)
    {
      m_circleBandChangedHandler(m_circles, changedRadiusIndex);
    }
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

void ImageViewWidget::wheelEvent(QWheelEvent* event)
{
  if (m_image.isNull())
  {
    QWidget::wheelEvent(event);
    return;
  }

  const QPoint widgetPoint = event->position().toPoint();
  const QRect oldDrawRect = imageDrawRect();
  if (!oldDrawRect.contains(widgetPoint))
  {
    QWidget::wheelEvent(event);
    return;
  }

  const int delta = event->angleDelta().y();
  if (delta == 0)
  {
    event->accept();
    return;
  }

  const QPointF imagePoint = widgetToImageF(widgetPoint);
  const double zoomStep = std::pow(1.0015, static_cast<double>(delta));
  const double previousZoom = m_zoomFactor;
  m_zoomFactor = std::clamp(m_zoomFactor * zoomStep, 1.0, 20.0);
  if (std::abs(m_zoomFactor - previousZoom) < 0.0001)
  {
    event->accept();
    return;
  }

  const QRect baseRect = baseImageDrawRect();
  const QSizeF newSize(baseRect.width() * m_zoomFactor, baseRect.height() * m_zoomFactor);
  const QPointF imageRatio(
    m_image.width() > 1 ? imagePoint.x() / static_cast<double>(m_image.width()) : 0.0,
    m_image.height() > 1 ? imagePoint.y() / static_cast<double>(m_image.height()) : 0.0);
  const QPointF desiredTopLeft = QPointF(widgetPoint) - QPointF(newSize.width() * imageRatio.x(), newSize.height() * imageRatio.y());
  const QPointF desiredCenter = desiredTopLeft + QPointF(newSize.width() * 0.5, newSize.height() * 0.5);
  m_panOffset = desiredCenter - QPointF(baseRect.center());
  clampZoomPan();

  update();
  event->accept();
}

