#pragma once

#include "gui/ImagePrimitives.h"
#include "gui/geometry/GeometryOverlay.h"

#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QWidget>
#include <QVector>
#include <functional>

class ImageViewWidget : public QWidget
{
public:
  explicit ImageViewWidget(QWidget* parent = nullptr);

  void setImage(const QPixmap& image);
  void clearImage();
  void setRoi(const QRect& imageRoi);
  void clearRoi();
  void setExclusionRects(const QVector<QRect>& imageRects);
  void clearExclusionRects();
  void setCircles(const QVector<ImageCircle>& imageCircles);
  void clearCircles();
  void setGeometryArea(const ImageRotatedRect& imageArea);
  void clearGeometryArea();
  void setGeometryPoints(const QVector<QPointF>& imagePoints);
  void clearGeometryPoints();
  void setGeometryLines(const QVector<ImageLine>& imageLines);
  void clearGeometryLines();
  void setGeometryOverlay(const GeometryOverlay& overlay);
  const GeometryOverlay& geometryOverlay() const;
  void clearGeometryOverlay();
  void setRoiDrawingEnabled(bool enabled);
  void setExclusionDrawingEnabled(bool enabled);
  void setGeometryAreaEditingEnabled(bool enabled);
  void setGeometryPointPickingEnabled(bool enabled);
  void setGeometryOverlayPointEditingEnabled(bool enabled);
  void setTwoPointLineDrawingEnabled(bool enabled);
  void setOuterCircleDrawingEnabled(bool enabled);
  void setInnerCircleDrawingEnabled(bool enabled);
  void setThreePointCircleDrawingEnabled(bool enabled);
  void setThreePointArcDrawingEnabled(bool enabled);
  void setRoiChangedHandler(std::function<void(const QRect&)> handler);
  void setExclusionRectAddedHandler(std::function<void(const QRect&)> handler);
  void setExclusionRectsChangedHandler(std::function<void(const QVector<QRect>&)> handler);
  void setCircleChangedHandler(std::function<void(bool, const ImageCircle&)> handler);
  void setThreePointCircleHandler(std::function<void(const QVector<QPoint>&)> handler);
  void setGeometryAreaChangedHandler(std::function<void(const ImageRotatedRect&)> handler);
  void setGeometryPointPickedHandler(std::function<void(const QPointF&)> handler);
  void setGeometryPointMovedHandler(std::function<void(const QPointF&)> handler);
  void setGeometryOverlayPointMovedHandler(std::function<void(int, const QPointF&)> handler);
  void setTwoPointLineHandler(std::function<void(const QVector<QPoint>&)> handler);

protected:
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  enum class ExclusionHandle
  {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
  };

  enum class GeometryAreaHandle
  {
    None,
    Move,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Rotate
  };

  QRect baseImageDrawRect() const;
  QRect imageDrawRect() const;
  void clampZoomPan();
  QPoint widgetToImage(const QPoint& widgetPoint) const;
  QRect widgetRectToImageRect(const QRect& widgetRect) const;
  QRect imageRectToWidgetRect(const QRect& imageRect) const;
  QRect imageCircleToWidgetRect(const ImageCircle& circle) const;
  QPoint imagePointToWidget(const QPoint& imagePoint) const;
  QPointF imagePointToWidget(const QPointF& imagePoint) const;
  QPointF widgetToImageF(const QPoint& widgetPoint) const;
  QVector<QPointF> geometryAreaCorners() const;
  QVector<QPointF> geometryAreaWidgetCorners() const;
  QPointF geometryAreaWidgetRotateHandle() const;
  GeometryAreaHandle geometryAreaHandleAt(const QPoint& widgetPoint) const;
  bool geometryAreaContains(const QPoint& widgetPoint) const;
  ImageRotatedRect normalizedGeometryArea(const ImageRotatedRect& area) const;
  int exclusionRectAt(const QPoint& widgetPoint) const;
  int exclusionHandleAt(const QPoint& widgetPoint, ExclusionHandle& handle) const;
  QRect clampImageRectToImage(const QRect& imageRect) const;

  enum class DrawingMode
  {
    None,
    Roi,
    Exclusion,
    OuterCircle,
    InnerCircle,
    ThreePointCircle,
    ThreePointArc,
    GeometryArea,
    GeometryPointPick,
    GeometryOverlayPointEdit,
    TwoPointLine
  };

  QPixmap m_image;
  double m_zoomFactor = 1.0;
  QPointF m_panOffset;
  QRect m_roi;
  QVector<QRect> m_exclusionRects;
  QVector<ImageCircle> m_circles;
  QVector<ImageLine> m_geometryLines;
  QVector<QPointF> m_geometryPoints;
  GeometryOverlay m_geometryOverlay;
  ImageRotatedRect m_geometryArea;
  bool m_hasRoi = false;
  bool m_hasGeometryArea = false;
  DrawingMode m_drawingMode = DrawingMode::None;
  bool m_dragging = false;
  bool m_movingExclusion = false;
  bool m_resizingExclusion = false;
  bool m_movingGeometryOverlayPoint = false;
  ExclusionHandle m_activeExclusionHandle = ExclusionHandle::None;
  GeometryAreaHandle m_activeGeometryAreaHandle = GeometryAreaHandle::None;
  int m_selectedExclusionIndex = -1;
  int m_selectedGeometryOverlayPointIndex = -1;
  QPoint m_dragStart;
  QPoint m_dragEnd;
  QPoint m_moveStartImagePoint;
  QRect m_moveStartImageRect;
  ImageRotatedRect m_moveStartGeometryArea;
  QVector<QPoint> m_threePointCirclePoints;
  QVector<QPoint> m_twoPointLinePoints;
  std::function<void(const QRect&)> m_roiChangedHandler;
  std::function<void(const QRect&)> m_exclusionRectAddedHandler;
  std::function<void(const QVector<QRect>&)> m_exclusionRectsChangedHandler;
  std::function<void(bool, const ImageCircle&)> m_circleChangedHandler;
  std::function<void(const QVector<QPoint>&)> m_threePointCircleHandler;
  std::function<void(const ImageRotatedRect&)> m_geometryAreaChangedHandler;
  std::function<void(const QPointF&)> m_geometryPointPickedHandler;
  std::function<void(const QPointF&)> m_geometryPointMovedHandler;
  std::function<void(int, const QPointF&)> m_geometryOverlayPointMovedHandler;
  std::function<void(const QVector<QPoint>&)> m_twoPointLineHandler;

  int geometryOverlayPointAt(const QPoint& widgetPoint) const;
};
