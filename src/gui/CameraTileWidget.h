#pragma once

#include "config/AppConfig.h"

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <functional>

class CameraTileWidget : public QFrame
{
public:
  explicit CameraTileWidget(const CameraConfig& camera, QWidget* parent = nullptr);

  const CameraConfig& camera() const;
  void setPreview(const QPixmap& preview);
  void setSelected(bool selected);
  void setClickHandler(std::function<void(const CameraConfig&)> handler);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void updatePreview();

  CameraConfig m_camera;
  QLabel* m_imageLabel = nullptr;
  QLabel* m_titleLabel = nullptr;
  QLabel* m_statusLabel = nullptr;
  QPixmap m_preview;
  bool m_selected = false;
  std::function<void(const CameraConfig&)> m_clickHandler;
};
