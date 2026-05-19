#pragma once

#include <QDateTime>
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVector>
#include <QPair>
#include <QString>

class MetricsPanelWidget : public QWidget
{
 public:
  explicit MetricsPanelWidget(QWidget* parent = nullptr);
  void addMetric(const QString& name, qint64 ms);
  void clearMetrics();
  bool saveMetricsToCsv(const QString& path) const;

private:
  QListWidget* m_list = nullptr;
  struct MetricEntry { QDateTime ts; QString name; qint64 ms; };
  QVector<MetricEntry> m_metrics;
};
