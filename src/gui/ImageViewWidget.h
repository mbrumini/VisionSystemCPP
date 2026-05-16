#pragma once

#include <QPixmap>
#include <QRect>
#include <QWidget>
#include <functional>

class ImageViewWidget : public QWidget
{
public:
  explicit ImageViewWidget(QWidget* parent = nullptr);

  void setImage(const QPixmap& image);
  void clearImage();
  void setRoi(const QRect& imageRoi);
  void clearRoi();
  void setRoiDrawingEnabled(bool enabled);
  void setRoiChangedHandler(std::function<void(const QRect&)> handler);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  QRect imageDrawRect() const;
  QPoint widgetToImage(const QPoint& widgetPoint) const;
  QRect widgetRectToImageRect(const QRect& widgetRect) const;
  QRect imageRectToWidgetRect(const QRect& imageRect) const;

  QPixmap m_image;
  QRect m_roi;
  bool m_hasRoi = false;
  bool m_roiDrawingEnabled = false;
  bool m_dragging = false;
  QPoint m_dragStart;
  QPoint m_dragEnd;
  std::function<void(const QRect&)> m_roiChangedHandler;
};
