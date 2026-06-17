#include "ImageViewWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPolygon>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

void ImageViewWidget::keyPressEvent(QKeyEvent* event)
{
  if (m_drawingMode == DrawingMode::Polygon)
  {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
      finishPolygonDrawing();
      return;
    }

    if (event->key() == Qt::Key_Escape)
    {
      m_pendingPolygon.clear();
      update();
      return;
    }
  }

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
  if (event->button() != Qt::LeftButton || m_image.isNull())
  {
    QWidget::mousePressEvent(event);
    return;
  }

  if (!imageDrawRect().contains(event->pos()))
  {
    return;
  }

  setFocus();

  if (m_geometryOverlayDimensionLabelMovedHandler)
  {
    const int labelIndex = geometryOverlayDimensionLabelAt(event->pos());
    if (labelIndex >= 0)
    {
      m_selectedGeometryOverlayDimensionIndex = labelIndex;
      m_movingGeometryOverlayDimensionLabel = true;
      m_dragging = true;
      setCursor(Qt::ClosedHandCursor);
      update();
      return;
    }
  }

  if (m_drawingMode == DrawingMode::None)
  {
    QWidget::mousePressEvent(event);
    return;
  }

  if (m_drawingMode == DrawingMode::Polygon)
  {
    const int vertexIndex = polygonVertexAt(event->pos());
    if (vertexIndex >= 0)
    {
      m_selectedPolygonVertexIndex = vertexIndex;
      m_movingPolygonVertex = true;
      m_dragging = true;
      update();
      return;
    }

    if (polygonContains(event->pos()))
    {
      m_movingPolygon = true;
      m_dragging = true;
      m_moveStartImagePoint = widgetToImage(event->pos());
      m_pendingPolygon.clear();
      update();
      return;
    }

    const QPoint imagePoint = widgetToImage(event->pos());
    if (m_pendingPolygon.size() >= 3)
    {
      const QPoint firstWidgetPoint = imagePointToWidget(m_pendingPolygon.first());
      if (QRect(firstWidgetPoint - QPoint(10, 10), QSize(20, 20)).contains(event->pos()))
      {
        finishPolygonDrawing();
        return;
      }
    }

    m_pendingPolygon.append(imagePoint);
    update();
    return;
  }

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

  if (m_drawingMode == DrawingMode::Roi && m_hasRoi)
  {
    ExclusionHandle handle = ExclusionHandle::None;
    if (roiHandleAt(event->pos(), handle))
    {
      m_activeRoiHandle = handle;
      m_resizingRoi = true;
      m_dragging = true;
      m_moveStartImageRect = m_roi;
      update();
      return;
    }

    if (roiContains(event->pos()))
    {
      m_movingRoi = true;
      m_dragging = true;
      m_moveStartImagePoint = widgetToImage(event->pos());
      m_moveStartImageRect = m_roi;
      update();
      return;
    }

    m_activeRoiHandle = ExclusionHandle::None;
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

  if (m_drawingMode == DrawingMode::ThreePointCircle || m_drawingMode == DrawingMode::ThreePointArc)
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

  if (m_movingGeometryOverlayDimensionLabel &&
      m_selectedGeometryOverlayDimensionIndex >= 0 &&
      m_selectedGeometryOverlayDimensionIndex < m_geometryOverlay.dimensions.size())
  {
    if (m_geometryOverlayDimensionLabelMovedHandler && imageDrawRect().contains(event->pos()))
    {
      const QPointF imagePoint = widgetToImageF(event->pos());
      GeometryOverlayDimension& dimension = m_geometryOverlay.dimensions[m_selectedGeometryOverlayDimensionIndex];
      dimension.labelPoint = imagePoint;
      dimension.hasLabelPoint = true;
      m_geometryOverlayDimensionLabelMovedHandler(dimension.id, imagePoint);
    }
    update();
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

  if (m_movingPolygonVertex &&
      m_selectedPolygonVertexIndex >= 0 &&
      m_selectedPolygonVertexIndex < m_searchPolygon.size())
  {
    m_searchPolygon[m_selectedPolygonVertexIndex] = widgetToImage(event->pos());
    if (m_searchPolygon.size() >= 3)
    {
      m_roi = QPolygon(m_searchPolygon).boundingRect().normalized();
      m_hasRoi = m_roi.isValid();
    }
    update();
    return;
  }

  if (m_movingPolygon && m_searchPolygon.size() >= 3)
  {
    const QPoint currentImagePoint = widgetToImage(event->pos());
    const QPoint delta = currentImagePoint - m_moveStartImagePoint;
    for (QPoint& point : m_searchPolygon)
    {
      point += delta;
    }
    m_moveStartImagePoint = currentImagePoint;
    m_roi = QPolygon(m_searchPolygon).boundingRect().normalized();
    m_hasRoi = m_roi.isValid();
    update();
    return;
  }

  if (m_movingRoi && m_hasRoi)
  {
    const QPoint currentImagePoint = widgetToImage(event->pos());
    const QPoint delta = currentImagePoint - m_moveStartImagePoint;
    m_roi = clampImageRectToImage(m_moveStartImageRect.translated(delta));
    update();
    return;
  }

  if (m_resizingRoi && m_hasRoi)
  {
    const QPoint currentImagePoint = widgetToImage(event->pos());
    QRect resized = m_moveStartImageRect;

    if (m_activeRoiHandle == ExclusionHandle::TopLeft)
    {
      resized.setTopLeft(currentImagePoint);
    }
    else if (m_activeRoiHandle == ExclusionHandle::TopRight)
    {
      resized.setTopRight(currentImagePoint);
    }
    else if (m_activeRoiHandle == ExclusionHandle::BottomLeft)
    {
      resized.setBottomLeft(currentImagePoint);
    }
    else if (m_activeRoiHandle == ExclusionHandle::BottomRight)
    {
      resized.setBottomRight(currentImagePoint);
    }

    resized = clampImageRectToImage(resized);
    if (resized.width() > 2 && resized.height() > 2)
    {
      m_roi = resized;
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

