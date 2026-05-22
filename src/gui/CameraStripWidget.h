#pragma once

#include "config/AppConfig.h"

#include <QFrame>
#include <QHash>
#include <QString>
#include <functional>

class QHBoxLayout;
class QToolButton;

class CameraStripWidget : public QFrame
{
public:
  explicit CameraStripWidget(QWidget* parent = nullptr);

  void setCameras(const QVector<CameraConfig>& cameras);
  void setSelectedCamera(const QString& cameraId);
  void setCameraBusy(const QString& cameraId, bool busy);
  void setCameraResult(const QString& cameraId, const QString& result);
  void setCameraClickHandler(std::function<void(const CameraConfig&)> handler);

private:
  void rebuild();
  void updateButton(const QString& cameraId);

  QVector<CameraConfig> m_cameras;
  QHash<QString, QToolButton*> m_buttons;
  QHash<QString, bool> m_busy;
  QHash<QString, QString> m_results;
  QString m_selectedCameraId;
  QHBoxLayout* m_layout = nullptr;
  std::function<void(const CameraConfig&)> m_clickHandler;
};
