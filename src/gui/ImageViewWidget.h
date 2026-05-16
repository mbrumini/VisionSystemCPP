#pragma once

#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>
#include <QVector>
#include <functional>

struct ImageCircle
{
  QPoint center;
  int radius = 0;
};

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
  void setRoiDrawingEnabled(bool enabled);
  void setExclusionDrawingEnabled(bool enabled);
  void setOuterCircleDrawingEnabled(bool enabled);
  void setInnerCircleDrawingEnabled(bool enabled);
  void setThreePointCircleDrawingEnabled(bool enabled);
  void setRoiChangedHandler(std::function<void(const QRect&)> handler);
  void setExclusionRectAddedHandler(std::function<void(const QRect&)> handler);
  void setExclusionRectsChangedHandler(std::function<void(const QVector<QRect>&)> handler);
  void setCircleChangedHandler(std::function<void(bool, const ImageCircle&)> handler);
  void setThreePointCircleHandler(std::function<void(const QVector<QPoint>&)> handler);

protected:
  void paintEvent(QPaintEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  enum class ExclusionHandle
  {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
  };

  QRect imageDrawRect() const;
  QPoint widgetToImage(const QPoint& widgetPoint) const;
  QRect widgetRectToImageRect(const QRect& widgetRect) const;
  QRect imageRectToWidgetRect(const QRect& imageRect) const;
  QRect imageCircleToWidgetRect(const ImageCircle& circle) const;
  QPoint imagePointToWidget(const QPoint& imagePoint) const;
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
    ThreePointCircle
  };

  QPixmap m_image;
  QRect m_roi;
  QVector<QRect> m_exclusionRects;
  QVector<ImageCircle> m_circles;
  bool m_hasRoi = false;
  DrawingMode m_drawingMode = DrawingMode::None;
  bool m_dragging = false;
  bool m_movingExclusion = false;
  bool m_resizingExclusion = false;
  ExclusionHandle m_activeExclusionHandle = ExclusionHandle::None;
  int m_selectedExclusionIndex = -1;
  QPoint m_dragStart;
  QPoint m_dragEnd;
  QPoint m_moveStartImagePoint;
  QRect m_moveStartImageRect;
  QVector<QPoint> m_threePointCirclePoints;
  std::function<void(const QRect&)> m_roiChangedHandler;
  std::function<void(const QRect&)> m_exclusionRectAddedHandler;
  std::function<void(const QVector<QRect>&)> m_exclusionRectsChangedHandler;
  std::function<void(bool, const ImageCircle&)> m_circleChangedHandler;
  std::function<void(const QVector<QPoint>&)> m_threePointCircleHandler;
};
