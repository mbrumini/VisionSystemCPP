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
enum class Column
{
  Camera = 0,
  Name,
  Nominal,
  ToleranceMinus,
  TolerancePlus,
  Current,
  Average,
  Minimum,
  Maximum,
  Scan,
  Count
};

QString measurementValue(const MeasurementResult& measurement)
{
  if (!measurement.valid)
  {
    return "N/D";
  }
  if (measurement.hasRealValue)
  {
    if (measurement.unit != "deg")
    {
      return QString("%1 %2 (%3 px)")
        .arg(measurement.valueReal, 0, 'f', 3)
        .arg(measurement.unit)
        .arg(measurement.valuePixels, 0, 'f', 3);
    }
    return QString("%1 %2").arg(measurement.valueReal, 0, 'f', 3).arg(measurement.unit);
  }
  const bool angleMeasurement =
    measurement.type == "line_line_angle" || measurement.type == "thread_phase";
  const QString suffix = angleMeasurement ? " deg" : " px";
  return QString("%1%2").arg(measurement.valuePixels, 0, 'f', 3).arg(suffix);
}

QString measurementName(const MeasurementResult& measurement)
{
  QString name = measurement.alias.trimmed().isEmpty()
    ? measurement.id
    : measurement.alias.trimmed();
  if (measurement.type == "line_line_distance")
  {
    if (measurement.criterion == "min")
    {
      name += " MIN";
    }
    else if (measurement.criterion == "max")
    {
      name += " MAX";
    }
    else
    {
      name += " MEDIA";
    }
  }
  else if (measurement.type == "arc_arc_distance_min")
  {
    name += " MIN";
  }
  return name;
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

QString statisticsValue(double value, const QString& unit)
{
  return QString("%1 %2").arg(value, 0, 'f', unit == "px" ? 1 : 3).arg(unit);
}

QString statisticsText(const CameraMeasurementResultRow& result, double value)
{
  return result.hasStatistics ? statisticsValue(value, result.statisticsUnit) : "-";
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

void fillMeasurementRow(QTableWidget* table,
                        int row,
                        const QString& cameraId,
                        const CameraMeasurementResultRow& result,
                        qint64 scanElapsedMs)
{
  const MeasurementResult& measurement = result.measurement;
  table->setItem(row, static_cast<int>(Column::Camera), new QTableWidgetItem(cameraId));
  table->setItem(row, static_cast<int>(Column::Name), new QTableWidgetItem(measurementName(measurement)));
  table->setItem(row, static_cast<int>(Column::Nominal), new QTableWidgetItem(
    configuredValue(measurement.nominal, measurement.hasNominal, measurement.unit)));
  table->setItem(row, static_cast<int>(Column::ToleranceMinus), new QTableWidgetItem(toleranceValue(measurement, false)));
  table->setItem(row, static_cast<int>(Column::TolerancePlus), new QTableWidgetItem(toleranceValue(measurement, true)));
  table->setItem(row, static_cast<int>(Column::Current), valueItem(measurement));
  table->setItem(row, static_cast<int>(Column::Average), new QTableWidgetItem(statisticsText(result, result.averageValue)));
  table->setItem(row, static_cast<int>(Column::Minimum), new QTableWidgetItem(statisticsText(result, result.minimumValue)));
  table->setItem(row, static_cast<int>(Column::Maximum), new QTableWidgetItem(statisticsText(result, result.maximumValue)));
  table->setItem(row, static_cast<int>(Column::Scan), new QTableWidgetItem(scanTime(scanElapsedMs)));
}

void updateMeasurementRow(QTableWidget* table, int row, const CameraMeasurementResultRow& result, qint64 scanElapsedMs)
{
  const MeasurementResult& measurement = result.measurement;
  if (QTableWidgetItem* value = table->item(row, static_cast<int>(Column::Current)))
  {
    value->setText(measurementValue(measurement));
    value->setForeground(measurementColor(measurement));
  }
  if (QTableWidgetItem* average = table->item(row, static_cast<int>(Column::Average)))
  {
    average->setText(statisticsText(result, result.averageValue));
  }
  if (QTableWidgetItem* minimum = table->item(row, static_cast<int>(Column::Minimum)))
  {
    minimum->setText(statisticsText(result, result.minimumValue));
  }
  if (QTableWidgetItem* maximum = table->item(row, static_cast<int>(Column::Maximum)))
  {
    maximum->setText(statisticsText(result, result.maximumValue));
  }
  if (QTableWidgetItem* scan = table->item(row, static_cast<int>(Column::Scan)))
  {
    scan->setText(scanTime(scanElapsedMs));
  }
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
  m_table->setColumnCount(static_cast<int>(Column::Count));
  m_table->setHorizontalHeaderLabels({
    "Camera",
    "Misura",
    "Nominale",
    "Toll -",
    "Toll +",
    "Misura attuale",
    "Media",
    "MIN",
    "MAX",
    "Scansione"
  });
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(static_cast<int>(Column::Current), QHeaderView::Stretch);
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
    CameraMeasurementResultRow result;
    result.measurement = measurements[row];
    fillMeasurementRow(m_table, row, cameraId, result, scanElapsedMs);
  }
}

void MeasurementResultsWidget::setAllCameraMeasurements(const QVector<CameraMeasurementResultRow>& measurements)
{
  m_title->setText("Misure | Tutte le telecamere");
  setCameraColumnVisible(true);

  if (m_table->rowCount() == measurements.size() && !measurements.isEmpty())
  {
    bool sameRows = true;
    for (int row = 0; row < measurements.size(); ++row)
    {
      const QTableWidgetItem* cameraItem = m_table->item(row, static_cast<int>(Column::Camera));
      const QTableWidgetItem* nameItem = m_table->item(row, static_cast<int>(Column::Name));
      if (!cameraItem || !nameItem)
      {
        sameRows = false;
        break;
      }
      const QString expectedShortKey = QString("%1|%2")
        .arg(measurements[row].cameraId, measurementName(measurements[row].measurement));
      const QString actualKey = QString("%1|%2").arg(cameraItem->text(), nameItem->text());
      if (actualKey != expectedShortKey)
      {
        sameRows = false;
        break;
      }
    }

    if (sameRows)
    {
      for (int row = 0; row < measurements.size(); ++row)
      {
        updateMeasurementRow(m_table, row, measurements[row], measurements[row].scanElapsedMs);
      }
      return;
    }
  }

  m_table->setRowCount(measurements.size());
  for (int row = 0; row < measurements.size(); ++row)
  {
    const CameraMeasurementResultRow& result = measurements[row];
    fillMeasurementRow(m_table, row, result.cameraId, result, result.scanElapsedMs);
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
  m_table->setColumnHidden(static_cast<int>(Column::Camera), !m_cameraColumnVisible);
  m_table->setColumnHidden(static_cast<int>(Column::Average), !m_cameraColumnVisible);
  m_table->setColumnHidden(static_cast<int>(Column::Minimum), !m_cameraColumnVisible);
  m_table->setColumnHidden(static_cast<int>(Column::Maximum), !m_cameraColumnVisible);
  m_table->setColumnHidden(static_cast<int>(Column::Scan), !m_showScanTime);
}
