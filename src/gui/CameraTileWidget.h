#pragma once

#include "config/AppConfig.h"

#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QVector>
#include <functional>

struct CameraTileMeasurementTableLabels
{
  QString emptyText;
  QString measurementColumn;
  QString currentColumn;
  QString averageColumn;
  QString minimumColumn;
  QString maximumColumn;
};

struct CameraTileMeasurementStat
{
  QString name;
  QString current;
  QString average;
  QString minimum;
  QString maximum;
  QString judgement;
};

class CameraTileWidget : public QFrame
{
public:
  explicit CameraTileWidget(const CameraConfig& camera, QWidget* parent = nullptr);

  const CameraConfig& camera() const;
  void setPreview(const QPixmap& preview);
  void setResultText(const QString& text);
  void setMeasurementStats(const CameraTileMeasurementTableLabels& labels,
                           const QVector<CameraTileMeasurementStat>& stats);
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
  QLabel* m_measurementStatsLabel = nullptr;
  QPixmap m_preview;
  bool m_selected = false;
  std::function<void(const CameraConfig&)> m_clickHandler;
};
