#include "MetricsPanelWidget.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>

MetricsPanelWidget::MetricsPanelWidget(QWidget* parent)
  : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_list = new QListWidget(this);
  m_list->setSelectionMode(QAbstractItemView::NoSelection);
  layout->addWidget(m_list, 1);

  auto* button = new QPushButton(tr("Export CSV"), this);
  button->setMinimumHeight(28);
  connect(button, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Export metrics"), QDir::homePath(), "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty())
    {
      return;
    }

    if (!saveMetricsToCsv(path))
    {
      // best-effort: ignore errors
    }
  });
  layout->addWidget(button);
}

void MetricsPanelWidget::addMetric(const QString& name, qint64 ms)
{
  const QDateTime now = QDateTime::currentDateTime();
  const QString timestamp = now.toString("HH:mm:ss");
  const QString text = name.isEmpty() ? QString("%1: %2 ms").arg(timestamp).arg(ms) : QString("%1 | %2: %3 ms").arg(timestamp).arg(name).arg(ms);
  m_list->insertItem(0, text);
  m_metrics.insert(0, MetricEntry{now, name, ms});
  // keep reasonable limit
  while (m_list->count() > 200)
  {
    delete m_list->takeItem(m_list->count() - 1);
  }
  while (m_metrics.size() > 200)
  {
    m_metrics.removeLast();
  }
}

void MetricsPanelWidget::clearMetrics()
{
  m_list->clear();
  m_metrics.clear();
}

bool MetricsPanelWidget::saveMetricsToCsv(const QString& path) const
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
  {
    return false;
  }

  QTextStream out(&file);
  out << "timestamp,name,ms\n";
  for (int i = m_metrics.size() - 1; i >= 0; --i)
  {
    const auto& e = m_metrics[i];
    out << e.ts.toString("yyyy-MM-dd HH:mm:ss") << "," << e.name << "," << e.ms << "\n";
  }

  return true;
}
