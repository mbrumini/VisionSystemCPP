#include "gui/MeasurementResultsWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QAbstractItemView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
QString measurementValue(const MeasurementResult& measurement)
{
  if (measurement.hasRealValue)
  {
    return QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  const QString suffix = measurement.type == "line_line_angle" ? " deg" : " px";
  return QString("%1%2").arg(measurement.valuePixels, 0, 'f', 3).arg(suffix);
}

QString measurementDiagnosticValue(const MeasurementResult& measurement)
{
  const QString suffix = measurement.type == "line_line_angle" ? " deg" : " px";
  return QString("%1%2").arg(measurement.valuePixels, 0, 'f', 3).arg(suffix);
}

QString measurementJudgement(const MeasurementResult& measurement)
{
  return measurement.judgement.isEmpty() ? "-" : measurement.judgement;
}
}

MeasurementResultsWidget::MeasurementResultsWidget(QWidget* parent)
  : QFrame(parent)
{
  setObjectName("measurementResults");
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 6, 10, 8);
  layout->setSpacing(5);

  m_title = new QLabel(this);
  m_title->setObjectName("measurementResultsTitle");
  layout->addWidget(m_title);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(8);
  m_table->setHorizontalHeaderLabels({"Camera", "ID", "Tipo", "A", "B", "Valore", "Pixel", "Esito"});
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setAlternatingRowColors(true);
  m_table->setMinimumHeight(110);
  m_table->setMaximumHeight(165);
  layout->addWidget(m_table);
}

void MeasurementResultsWidget::setTitle(const QString& title)
{
  m_title->setText(title);
}

void MeasurementResultsWidget::setMeasurements(const QString& cameraId, const QVector<MeasurementResult>& measurements)
{
  m_title->setText(cameraId.isEmpty() ? "Misure" : QString("Misure | %1").arg(cameraId));
  setCameraColumnVisible(false);
  m_table->setRowCount(measurements.size());
  for (int row = 0; row < measurements.size(); ++row)
  {
    const MeasurementResult& measurement = measurements[row];
    m_table->setItem(row, 0, new QTableWidgetItem(cameraId));
    m_table->setItem(row, 1, new QTableWidgetItem(measurement.id));
    m_table->setItem(row, 2, new QTableWidgetItem(measurement.type));
    m_table->setItem(row, 3, new QTableWidgetItem(measurement.sourceAId));
    m_table->setItem(row, 4, new QTableWidgetItem(measurement.sourceBId));
    m_table->setItem(row, 5, new QTableWidgetItem(measurementValue(measurement)));
    m_table->setItem(row, 6, new QTableWidgetItem(measurementDiagnosticValue(measurement)));
    m_table->setItem(row, 7, new QTableWidgetItem(measurementJudgement(measurement)));
  }
}

void MeasurementResultsWidget::setAllCameraMeasurements(const QVector<CameraMeasurementResultRow>& measurements)
{
  m_title->setText("Misure | Tutte le telecamere");
  setCameraColumnVisible(true);
  m_table->setRowCount(measurements.size());
  for (int row = 0; row < measurements.size(); ++row)
  {
    const CameraMeasurementResultRow& result = measurements[row];
    const MeasurementResult& measurement = result.measurement;
    m_table->setItem(row, 0, new QTableWidgetItem(result.cameraId));
    m_table->setItem(row, 1, new QTableWidgetItem(measurement.id));
    m_table->setItem(row, 2, new QTableWidgetItem(measurement.type));
    m_table->setItem(row, 3, new QTableWidgetItem(measurement.sourceAId));
    m_table->setItem(row, 4, new QTableWidgetItem(measurement.sourceBId));
    m_table->setItem(row, 5, new QTableWidgetItem(measurementValue(measurement)));
    m_table->setItem(row, 6, new QTableWidgetItem(measurementDiagnosticValue(measurement)));
    m_table->setItem(row, 7, new QTableWidgetItem(measurementJudgement(measurement)));
  }
}

void MeasurementResultsWidget::setCameraColumnVisible(bool visible)
{
  m_table->setColumnHidden(0, !visible);
}
