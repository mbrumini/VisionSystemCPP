#include "ImageViewWidget.h"

#include <utility>

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
  m_hasDetectedCircle = false;
  m_geometryPoints.clear();
  m_geometryLines.clear();
  m_geometryOverlay.clear();
  m_searchPolygon.clear();
  m_pendingPolygon.clear();
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

void ImageViewWidget::setSearchPolygon(const QVector<QPoint>& imagePolygon)
{
  m_searchPolygon.clear();
  for (const QPoint& point : imagePolygon)
  {
    m_searchPolygon.append(point);
  }
  m_selectedPolygonVertexIndex = -1;
  update();
}

void ImageViewWidget::clearSearchPolygon()
{
  m_searchPolygon.clear();
  m_pendingPolygon.clear();
  m_selectedPolygonVertexIndex = -1;
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
  if (m_circleBandEditing)
  {
    return;
  }
  m_circles.clear();
  update();
}

void ImageViewWidget::setDetectedCircle(const ImageCircle& circle)
{
  m_detectedCircle = circle;
  m_hasDetectedCircle = circle.radius > 0;
  update();
}

void ImageViewWidget::clearDetectedCircle()
{
  m_detectedCircle = {};
  m_hasDetectedCircle = false;
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

const GeometryOverlay& ImageViewWidget::geometryOverlay() const
{
  return m_geometryOverlay;
}

void ImageViewWidget::clearGeometryOverlay()
{
  m_geometryOverlay.clear();
  m_searchPolygon.clear();
  m_pendingPolygon.clear();
  update();
}

void ImageViewWidget::setRoiDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Roi : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_selectedGeometryOverlayDimensionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setPolygonDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Polygon : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingPolygon = false;
  m_movingPolygonVertex = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_selectedPolygonVertexIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_selectedGeometryOverlayDimensionIndex = -1;
  m_pendingPolygon.clear();
  update();
}

void ImageViewWidget::setExclusionDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::Exclusion : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingPolygon = false;
  m_movingPolygonVertex = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setGeometryAreaEditingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::GeometryArea : DrawingMode::None;
  setCursor(enabled ? Qt::SizeAllCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
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
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_selectedGeometryOverlayDimensionIndex = -1;
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
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_movingGeometryOverlayDimensionLabel = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_selectedGeometryOverlayPointIndex = -1;
  m_selectedGeometryOverlayDimensionIndex = -1;
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
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_activeRoiHandle = ExclusionHandle::None;
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
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setInnerCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::InnerCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setCircleBandEditingEnabled(bool enabled)
{
  m_circleBandEditing = enabled;
  m_drawingMode = enabled ? DrawingMode::CircleBandEdit : DrawingMode::None;
  setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingCircleBandCenter = false;
  m_selectedCircleBandRadius = -1;
  update();
}

void ImageViewWidget::setThreePointCircleDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::ThreePointCircle : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_selectedExclusionIndex = -1;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setThreePointArcDrawingEnabled(bool enabled)
{
  m_drawingMode = enabled ? DrawingMode::ThreePointArc : DrawingMode::None;
  setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  m_dragging = false;
  m_movingRoi = false;
  m_resizingRoi = false;
  m_movingExclusion = false;
  m_resizingExclusion = false;
  m_movingGeometryOverlayPoint = false;
  m_activeRoiHandle = ExclusionHandle::None;
  m_threePointCirclePoints.clear();
  update();
}

void ImageViewWidget::setRoiChangedHandler(std::function<void(const QRect&)> handler)
{
  m_roiChangedHandler = std::move(handler);
}

void ImageViewWidget::setPolygonChangedHandler(std::function<void(const QVector<QPoint>&)> handler)
{
  m_polygonChangedHandler = std::move(handler);
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

void ImageViewWidget::setCircleBandChangedHandler(std::function<void(const QVector<ImageCircle>&, int)> handler)
{
  m_circleBandChangedHandler = std::move(handler);
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

void ImageViewWidget::setGeometryOverlayDimensionLabelMovedHandler(std::function<void(const QString&, const QPointF&)> handler)
{
  m_geometryOverlayDimensionLabelMovedHandler = std::move(handler);
}

void ImageViewWidget::setTwoPointLineHandler(std::function<void(const QVector<QPoint>&)> handler)
{
  m_twoPointLineHandler = std::move(handler);
}

