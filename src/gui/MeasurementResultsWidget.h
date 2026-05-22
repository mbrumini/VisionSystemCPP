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
};

class MeasurementResultsWidget : public QFrame
{
public:
  explicit MeasurementResultsWidget(QWidget* parent = nullptr);

  void setTitle(const QString& title);
  void setMeasurements(const QString& cameraId, const QVector<MeasurementResult>& measurements);
  void setAllCameraMeasurements(const QVector<CameraMeasurementResultRow>& measurements);

private:
  void setCameraColumnVisible(bool visible);

  QLabel* m_title = nullptr;
  QTableWidget* m_table = nullptr;
};
