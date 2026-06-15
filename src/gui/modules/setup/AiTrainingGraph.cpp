#include "gui/modules/setup/AiTrainingGraph.h"

#include "config/RecipeManager.h"
#include "gui/modules/setup/AiClassificationPaths.h"

#include <QFile>
#include <QPainter>
#include <QTextStream>

namespace
{
int csvIndex(const QStringList& header, const QString& name)
{
  for (int i = 0; i < header.size(); ++i)
  {
    if (header[i].trimmed() == name)
    {
      return i;
    }
  }
  return -1;
}

double csvValue(const QStringList& values, int index)
{
  if (index < 0 || index >= values.size())
  {
    return 0.0;
  }
  return values[index].trimmed().toDouble();
}

QVector<double> normalizedSeries(const QVector<double>& values)
{
  if (values.isEmpty())
  {
    return {};
  }

  double minValue = values.first();
  double maxValue = values.first();
  for (double value : values)
  {
    minValue = qMin(minValue, value);
    maxValue = qMax(maxValue, value);
  }

  QVector<double> result;
  result.reserve(values.size());
  const double range = maxValue - minValue;
  for (double value : values)
  {
    result.append(range <= 0.000001 ? 0.5 : (value - minValue) / range);
  }
  return result;
}

void drawSeries(QPainter& painter, const QRect& chart, const QVector<double>& values, const QColor& color)
{
  if (values.size() < 2)
  {
    return;
  }

  const QVector<double> normalized = normalizedSeries(values);
  QPolygonF polyline;
  polyline.reserve(normalized.size());
  for (int i = 0; i < normalized.size(); ++i)
  {
    const double x = chart.left() + (chart.width() * i) / qMax(1, normalized.size() - 1);
    const double y = chart.bottom() - normalized[i] * chart.height();
    polyline.append(QPointF(x, y));
  }

  painter.setPen(QPen(color, 3));
  painter.drawPolyline(polyline);
}

QVector<double> lastValues(const QVector<double>& values, int count)
{
  if (values.size() <= count)
  {
    return values;
  }
  return values.mid(values.size() - count);
}

void drawTrainingLegend(QPainter& painter, const QRect& chart)
{
  QFont legendFont = painter.font();
  legendFont.setPointSize(18);
  legendFont.setBold(false);
  painter.setFont(legendFont);
  const int legendY = chart.bottom() + 34;
  painter.setPen(QPen(QColor("#3fb950"), 4));
  painter.drawLine(chart.left(), legendY, chart.left() + 40, legendY);
  painter.setPen(QColor("#e6edf3"));
  painter.drawText(chart.left() + 50, legendY + 7, "Accuracy");
  painter.setPen(QPen(QColor("#f778ba"), 4));
  painter.drawLine(chart.left() + 210, legendY, chart.left() + 250, legendY);
  painter.setPen(QColor("#e6edf3"));
  painter.drawText(chart.left() + 260, legendY + 7, "Train loss");
  painter.setPen(QPen(QColor("#f2cc60"), 4));
  painter.drawLine(chart.left() + 430, legendY, chart.left() + 470, legendY);
  painter.setPen(QColor("#e6edf3"));
  painter.drawText(chart.left() + 480, legendY + 7, "Val loss");
}
}

QPixmap renderAiClassificationTrainingGraph(const RecipeManager& recipes, const QString& cameraId)
{
  const QString resultsPath = aiNewestClassificationResultsPath(recipes, cameraId);
  QPixmap pixmap(1280, 720);
  pixmap.fill(QColor("#101418"));

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRect chart(90, 90, 1040, 520);

  painter.setPen(QPen(QColor("#56616c"), 2));
  painter.drawRect(chart);
  for (int i = 1; i < 5; ++i)
  {
    const int y = chart.top() + chart.height() * i / 5;
    painter.drawLine(chart.left(), y, chart.right(), y);
  }

  painter.setPen(QColor("#e6edf3"));
  QFont titleFont = painter.font();
  titleFont.setPointSize(26);
  titleFont.setBold(true);
  painter.setFont(titleFont);
  painter.drawText(QRect(90, 25, 1000, 40), Qt::AlignLeft | Qt::AlignVCenter, QString("Training AI - %1").arg(cameraId));

  QVector<double> epochs;
  QVector<double> trainLoss;
  QVector<double> valLoss;
  QVector<double> top1;

  QFile file(resultsPath);
  if (!resultsPath.isEmpty() && file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream stream(&file);
    const QStringList header = stream.readLine().split(',');
    const int epochIndex = csvIndex(header, "epoch");
    const int trainLossIndex = csvIndex(header, "train/loss");
    const int valLossIndex = csvIndex(header, "val/loss");
    const int top1Index = csvIndex(header, "metrics/accuracy_top1");

    while (!stream.atEnd())
    {
      const QString line = stream.readLine().trimmed();
      if (line.isEmpty())
      {
        continue;
      }
      const QStringList values = line.split(',');
      epochs.append(csvValue(values, epochIndex));
      trainLoss.append(csvValue(values, trainLossIndex));
      valLoss.append(csvValue(values, valLossIndex));
      top1.append(csvValue(values, top1Index));
    }
  }

  if (epochs.isEmpty())
  {
    painter.setPen(QColor("#aab6c2"));
    QFont textFont = painter.font();
    textFont.setPointSize(18);
    textFont.setBold(false);
    painter.setFont(textFont);
    painter.drawText(QRect(chart.left(), chart.top(), chart.width(), chart.height()), Qt::AlignCenter,
                     "In attesa dei dati training...");
  }
  else
  {
    const int maxVisibleEpochs = 25;
    const QVector<double> visibleEpochs = lastValues(epochs, maxVisibleEpochs);
    const QVector<double> visibleTop1 = lastValues(top1, maxVisibleEpochs);
    const QVector<double> visibleTrainLoss = lastValues(trainLoss, maxVisibleEpochs);
    const QVector<double> visibleValLoss = lastValues(valLoss, maxVisibleEpochs);

    drawSeries(painter, chart, visibleTop1, QColor("#3fb950"));
    drawSeries(painter, chart, visibleTrainLoss, QColor("#f778ba"));
    drawSeries(painter, chart, visibleValLoss, QColor("#f2cc60"));

    painter.setPen(QColor("#e6edf3"));
    QFont textFont = painter.font();
    textFont.setPointSize(13);
    textFont.setBold(false);
    painter.setFont(textFont);

    const int tickCount = qMin(6, visibleEpochs.size());
    for (int i = 0; i < tickCount; ++i)
    {
      const int sourceIndex = tickCount <= 1
        ? 0
        : qRound(static_cast<double>(i) * (visibleEpochs.size() - 1) / (tickCount - 1));
      const double x = chart.left() + (chart.width() * sourceIndex) / qMax(1, visibleEpochs.size() - 1);
      painter.setPen(QPen(QColor("#56616c"), 1));
      painter.drawLine(QPointF(x, chart.bottom()), QPointF(x, chart.bottom() + 7));
      painter.setPen(QColor("#cdd6df"));
      painter.drawText(QRectF(x - 24, chart.bottom() + 10, 48, 22), Qt::AlignCenter, QString::number(static_cast<int>(visibleEpochs[sourceIndex])));
    }
  }

  drawTrainingLegend(painter, chart);
  return pixmap;
}
