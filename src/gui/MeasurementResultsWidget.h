#pragma once

#include "measurement/MeasurementGeometry.h"

#include <QFrame>
#include <QString>
#include <QVector>

class QLabel;
class QTableWidget;

struct CameraMeasurementResultRow
{
  QString cameraId;
  MeasurementResult measurement;
  qint64 scanElapsedMs = -1;
};

class MeasurementResultsWidget : public QFrame
{
public:
  explicit MeasurementResultsWidget(QWidget* parent = nullptr);

  void setTitle(const QString& title);
  void setShowScanTime(bool visible);
  void setMeasurements(const QString& cameraId,
                       const QVector<MeasurementResult>& measurements,
                       qint64 scanElapsedMs = -1);
  void setAllCameraMeasurements(const QVector<CameraMeasurementResultRow>& measurements);

private:
  void setCameraColumnVisible(bool visible);
  void updateColumnVisibility();

  QLabel* m_title = nullptr;
  QTableWidget* m_table = nullptr;
  bool m_cameraColumnVisible = false;
  bool m_showScanTime = false;
};
