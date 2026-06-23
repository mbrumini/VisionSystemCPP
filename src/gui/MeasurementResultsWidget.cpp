#include "gui/MeasurementResultsWidget.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
QString measurementValue(const MeasurementResult& measurement)
{
  if (!measurement.valid)
  {
    return "N/D";
  }
  if (measurement.hasRealValue)
  {
    return QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  const QString suffix = measurement.type == "line_line_angle" ? " deg" : " px";
  return QString("%1%2").arg(measurement.valuePixels, 0, 'f', 3).arg(suffix);
}

QString measurementName(const MeasurementResult& measurement)
{
  return measurement.alias.trimmed().isEmpty()
    ? measurement.id
    : measurement.alias.trimmed();
}

QString configuredValue(double value, bool configured, const QString& unit)
{
  return configured
    ? QString("%1 %2").arg(value, 0, 'f', 3).arg(unit)
    : "-";
}

QString toleranceValue(const MeasurementResult& measurement, bool upper)
{
  const bool configured = measurement.hasNominal && (upper ? measurement.hasMax : measurement.hasMin);
  if (!configured)
  {
    return "-";
  }

  const double delta = upper
    ? measurement.max - measurement.nominal
    : measurement.nominal - measurement.min;
  return QString("%1%2 %3")
    .arg(upper ? "+" : "-")
    .arg(delta, 0, 'f', 3)
    .arg(measurement.unit);
}

QColor measurementColor(const MeasurementResult& measurement)
{
  if (!measurement.valid)
  {
    return QColor("#9aa4ad");
  }
  if (measurement.judgement == "OK")
  {
    return QColor("#35c46a");
  }
  if (measurement.judgement == "NOK")
  {
    return QColor("#ff2f2f");
  }
  return QColor("#ff8a00");
}

QTableWidgetItem* valueItem(const MeasurementResult& measurement)
{
  auto* item = new QTableWidgetItem(measurementValue(measurement));
  item->setForeground(measurementColor(measurement));
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  if (!measurement.valid)
  {
    item->setToolTip("Misura non disponibile");
  }
  else
  {
    item->setToolTip(measurement.judgement.isEmpty() ? "Tolleranza non valutata" : measurement.judgement);
  }
  return item;
}

QString scanTime(qint64 elapsedMs)
{
  return elapsedMs >= 0 ? QString("%1 ms").arg(elapsedMs) : "-";
}
}

MeasurementResultsWidget::MeasurementResultsWidget(QWidget* parent)
  : QFrame(parent)
{
  setObjectName("measurementResults");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 6, 10, 8);
  layout->setSpacing(5);

  m_title = new QLabel(this);
  m_title->setObjectName("measurementResultsTitle");
  layout->addWidget(m_title);

  m_table = new QTableWidget(this);
  m_table->setColumnCount(7);
  m_table->setHorizontalHeaderLabels({
    "Camera",
    "Misura",
    "Nominale",
    "Toll -",
    "Toll +",
    "Misura attuale",
    "Scansione"
  });
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setAlternatingRowColors(true);
  layout->addWidget(m_table, 1);
  setExpanded(true);
}

void MeasurementResultsWidget::setTitle(const QString& title)
{
  m_title->setText(title);
}

void MeasurementResultsWidget::setShowScanTime(bool visible)
{
  m_showScanTime = visible;
  updateColumnVisibility();
}

void MeasurementResultsWidget::setMeasurements(const QString& cameraId,
                                               const QVector<MeasurementResult>& measurements,
                                               qint64 scanElapsedMs)
{
  m_title->setText(cameraId.isEmpty() ? "Misure" : QString("Misure | %1").arg(cameraId));
  setCameraColumnVisible(false);
  m_table->setRowCount(measurements.size());
  for (int row = 0; row < measurements.size(); ++row)
  {
    const MeasurementResult& measurement = measurements[row];
    m_table->setItem(row, 0, new QTableWidgetItem(cameraId));
    m_table->setItem(row, 1, new QTableWidgetItem(measurementName(measurement)));
    m_table->setItem(row, 2, new QTableWidgetItem(
      configuredValue(measurement.nominal, measurement.hasNominal, measurement.unit)));
    m_table->setItem(row, 3, new QTableWidgetItem(toleranceValue(measurement, false)));
    m_table->setItem(row, 4, new QTableWidgetItem(toleranceValue(measurement, true)));
    m_table->setItem(row, 5, valueItem(measurement));
    m_table->setItem(row, 6, new QTableWidgetItem(scanTime(scanElapsedMs)));
  }
}

void MeasurementResultsWidget::setAllCameraMeasurements(const QVector<CameraMeasurementResultRow>& measurements)
{
  m_title->setText("Misure | Tutte le telecamere");
  setCameraColumnVisible(true);

  auto rowKey = [](const CameraMeasurementResultRow& result) {
    const MeasurementResult& measurement = result.measurement;
    return QString("%1|%2|%3|%4|%5")
      .arg(result.cameraId, measurement.type, measurement.sourceAId, measurement.sourceBId, measurement.id);
  };

  if (m_table->rowCount() == measurements.size() && !measurements.isEmpty())
  {
    bool sameRows = true;
    for (int row = 0; row < measurements.size(); ++row)
    {
      const QTableWidgetItem* cameraItem = m_table->item(row, 0);
      const QTableWidgetItem* nameItem = m_table->item(row, 1);
      if (!cameraItem || !nameItem)
      {
        sameRows = false;
        break;
      }
      const QString expectedKey = rowKey(measurements[row]);
      const QString actualKey = QString("%1|%2")
        .arg(cameraItem->text(), nameItem->text());
      const QString expectedShortKey = QString("%1|%2")
        .arg(measurements[row].cameraId, measurementName(measurements[row].measurement));
      if (actualKey != expectedShortKey)
      {
        sameRows = false;
        break;
      }
      Q_UNUSED(expectedKey);
    }

    if (sameRows)
    {
      for (int row = 0; row < measurements.size(); ++row)
      {
        const CameraMeasurementResultRow& result = measurements[row];
        const MeasurementResult& measurement = result.measurement;
        if (QTableWidgetItem* value = m_table->item(row, 5))
        {
          value->setText(measurementValue(measurement));
          value->setForeground(measurementColor(measurement));
        }
        if (QTableWidgetItem* scan = m_table->item(row, 6))
        {
          scan->setText(scanTime(result.scanElapsedMs));
        }
      }
      return;
    }
  }

  m_table->setRowCount(measurements.size());
  for (int row = 0; row < measurements.size(); ++row)
  {
    const CameraMeasurementResultRow& result = measurements[row];
    const MeasurementResult& measurement = result.measurement;
    m_table->setItem(row, 0, new QTableWidgetItem(result.cameraId));
    m_table->setItem(row, 1, new QTableWidgetItem(measurementName(measurement)));
    m_table->setItem(row, 2, new QTableWidgetItem(
      configuredValue(measurement.nominal, measurement.hasNominal, measurement.unit)));
    m_table->setItem(row, 3, new QTableWidgetItem(toleranceValue(measurement, false)));
    m_table->setItem(row, 4, new QTableWidgetItem(toleranceValue(measurement, true)));
    m_table->setItem(row, 5, valueItem(measurement));
    m_table->setItem(row, 6, new QTableWidgetItem(scanTime(result.scanElapsedMs)));
  }
}

void MeasurementResultsWidget::setExpanded(bool expanded)
{
  m_expanded = expanded;
  if (!m_table)
  {
    return;
  }

  if (expanded)
  {
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_table->setMinimumHeight(160);
    m_table->setMaximumHeight(QWIDGETSIZE_MAX);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  }
  else
  {
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_table->setMinimumHeight(110);
    m_table->setMaximumHeight(165);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  }
}

void MeasurementResultsWidget::setCameraColumnVisible(bool visible)
{
  m_cameraColumnVisible = visible;
  updateColumnVisibility();
}

void MeasurementResultsWidget::updateColumnVisibility()
{
  m_table->setColumnHidden(0, !m_cameraColumnVisible);
  m_table->setColumnHidden(6, !m_showScanTime);
}
