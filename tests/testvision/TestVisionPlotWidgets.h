#pragma once

#include "TestVisionTypes.h"
#include "TestVisionMath.h"

#include <QPainter>
#include <QPen>
#include <QWidget>

class PlotWidget : public QWidget
{
public:
  enum class Mode { Angle, CenterError };

  PlotWidget(QString title, Mode mode, QWidget* parent = nullptr)
    : QWidget(parent), m_title(std::move(title)), m_mode(mode)
  {
    setMinimumHeight(230);
  }

  void setResults(const QVector<TestResult>& results)
  {
    m_results = results;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#0f1419"));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#e8edf2"));
    painter.drawText(QRect(12, 8, width() - 24, 24), Qt::AlignLeft, m_title);

    const QRectF area(52, 38, width() - 70, height() - 72);
    painter.setPen(QColor("#596572"));
    painter.drawRect(area);
    if (m_results.isEmpty())
    {
      painter.drawText(area, Qt::AlignCenter, "Nessun dato");
      return;
    }

    double minX = m_results.first().expectedAngle;
    double maxX = minX;
    double minY = m_mode == Mode::Angle
      ? testVisionNormalizePcaAngleNear(
          m_results.first().actualAngle,
          m_results.first().expectedAngle)
      : 0.0;
    double maxY = minY;
    for (const TestResult& result : m_results)
    {
      minX = std::min(minX, result.expectedAngle);
      maxX = std::max(maxX, result.expectedAngle);
      const double y = m_mode == Mode::Angle
        ? testVisionNormalizePcaAngleNear(result.actualAngle, result.expectedAngle)
        : result.centerError;
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
    if (m_mode == Mode::Angle)
    {
      const double commonMinimum = std::min(minX, minY);
      const double commonMaximum = std::max(maxX, maxY);
      minX = commonMinimum;
      minY = commonMinimum;
      maxX = commonMaximum;
      maxY = commonMaximum;
    }
    if (std::abs(maxX - minX) < 0.001) maxX = minX + 1.0;
    if (std::abs(maxY - minY) < 0.001) maxY = minY + 1.0;

    auto mapPoint = [&](double x, double y) {
      return QPointF(
        area.left() + (x - minX) / (maxX - minX) * area.width(),
        area.bottom() - (y - minY) / (maxY - minY) * area.height());
    };

    const QVector<QColor> colors = {
      QColor("#4da3ff"), QColor("#40c980"), QColor("#ffad42"),
      QColor("#d576ff"), QColor("#ff667a")
    };
    if (m_mode == Mode::Angle)
    {
      painter.setPen(QPen(QColor("#8b949e"), 2, Qt::DashLine));
      painter.drawLine(mapPoint(minX, minX), mapPoint(maxX, maxX));
      painter.drawText(
        QRectF(area.left() + 8, area.top() + 6, 190, 20),
        Qt::AlignLeft,
        "Ideale: atteso = rilevato");
    }
    QMap<int, QVector<TestResult>> byPass;
    for (const TestResult& result : m_results)
    {
      byPass[result.pose.pass].append(result);
    }
    int colorIndex = 0;
    for (auto it = byPass.cbegin(); it != byPass.cend(); ++it, ++colorIndex)
    {
      painter.setPen(QPen(colors[colorIndex % colors.size()], 2));
      QPointF previous;
      bool hasPrevious = false;
      for (const TestResult& result : it.value())
      {
        const double y = m_mode == Mode::Angle
          ? testVisionNormalizePcaAngleNear(result.actualAngle, result.expectedAngle)
          : result.centerError;
        const QPointF point = mapPoint(result.expectedAngle, y);
        if (hasPrevious) painter.drawLine(previous, point);
        const bool outsideTolerance =
          m_mode == Mode::Angle && result.angleError > 5.0;
        painter.setPen(QPen(
          outsideTolerance ? QColor("#ff4d5a") : colors[colorIndex % colors.size()],
          outsideTolerance ? 3 : 2));
        painter.setBrush(outsideTolerance
          ? QColor("#ff4d5a")
          : colors[colorIndex % colors.size()]);
        painter.drawEllipse(point, outsideTolerance ? 5 : 4, outsideTolerance ? 5 : 4);
        painter.setPen(QPen(colors[colorIndex % colors.size()], 2));
        previous = point;
        hasPrevious = true;
      }
      painter.drawText(
        QRectF(area.right() - 72, area.top() + colorIndex * 18, 68, 18),
        Qt::AlignRight,
        QString("Giro %1").arg(it.key()));
    }

    painter.setPen(QColor("#aab4be"));
    painter.drawText(8, area.top() + 12, QString::number(maxY, 'f', 1));
    painter.drawText(8, area.bottom(), QString::number(minY, 'f', 1));
    painter.drawText(area.left(), height() - 12, QString::number(minX, 'f', 1) + "°");
    painter.drawText(area.right() - 45, height() - 12, QString::number(maxX, 'f', 1) + "°");
  }

private:
  QString m_title;
  Mode m_mode;
  QVector<TestResult> m_results;
};

class RepeatabilityPlotWidget : public QWidget
{
public:
  explicit RepeatabilityPlotWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
    setMinimumHeight(210);
  }

  void setResults(const QVector<TestResult>& results)
  {
    m_results = results;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#0f1419"));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#e8edf2"));
    painter.drawText(QRect(12, 8, width() - 24, 24), Qt::AlignLeft,
                     "Delta centro rispetto al giro 1");
    const QRectF area(52, 38, width() - 70, height() - 65);
    painter.setPen(QColor("#596572"));
    painter.drawRect(area);

    QMap<int, TestResult> references;
    for (const TestResult& result : m_results)
      if (result.pose.pass == 1) references[result.pose.poseIndex] = result;

    struct Delta { int pose; int pass; double value; };
    QVector<Delta> deltas;
    double maximum = 0.001;
    int maxPose = 1;
    for (const TestResult& result : m_results)
    {
      if (result.pose.pass <= 1 || !references.contains(result.pose.poseIndex)) continue;
      const TestResult& reference = references[result.pose.poseIndex];
      const double value = std::hypot(
        result.actualX - reference.actualX,
        result.actualY - reference.actualY);
      deltas.append({result.pose.poseIndex, result.pose.pass, value});
      maximum = std::max(maximum, value);
      maxPose = std::max(maxPose, result.pose.poseIndex);
    }
    if (deltas.isEmpty())
    {
      painter.drawText(area, Qt::AlignCenter, "Servono almeno 2 giri");
      return;
    }

    const QVector<QColor> colors = {QColor("#40c980"), QColor("#ffad42"), QColor("#d576ff")};
    for (int pass = 2; pass <= 20; ++pass)
    {
      painter.setPen(QPen(colors[(pass - 2) % colors.size()], 2));
      QPointF previous;
      bool hasPrevious = false;
      for (const Delta& delta : deltas)
      {
        if (delta.pass != pass) continue;
        const QPointF current(
          area.left() + (delta.pose - 1.0) / std::max(1, maxPose - 1) * area.width(),
          area.bottom() - delta.value / maximum * area.height());
        if (hasPrevious) painter.drawLine(previous, current);
        painter.setBrush(colors[(pass - 2) % colors.size()]);
        painter.drawEllipse(current, 4, 4);
        previous = current;
        hasPrevious = true;
      }
    }
    painter.setPen(QColor("#aab4be"));
    painter.drawText(8, area.top() + 12, QString::number(maximum, 'f', 4));
    painter.drawText(8, area.bottom(), "0");
  }

private:
  QVector<TestResult> m_results;
};

class MeasurementStabilityPlotWidget : public QWidget
{
public:
  explicit MeasurementStabilityPlotWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
    setMinimumHeight(230);
  }

  void setResults(const QVector<TestResult>& results)
  {
    m_results = results;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#0f1419"));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#e8edf2"));
    painter.drawText(
      QRect(12, 8, width() - 24, 24),
      Qt::AlignLeft,
      "Angolo atteso → errore misura (px)");
    const QRectF area(52, 38, width() - 70, height() - 72);
    painter.setPen(QColor("#596572"));
    painter.drawRect(area);

    struct Point
    {
      double angle = 0.0;
      double error = 0.0;
      QString id;
      int pass = 1;
    };
    QVector<Point> points;
    for (const TestResult& result : m_results)
    {
      if (!result.valid)
      {
        continue;
      }
      for (const TestMeasurementReading& reading : result.measurements)
      {
        if (!reading.valid || !reading.hasExpectedBaseline)
        {
          continue;
        }
        points.append({
          result.expectedAngle,
          reading.baselineErrorPixels,
          reading.id,
          result.pose.pass
        });
      }
    }
    if (points.isEmpty())
    {
      painter.drawText(area, Qt::AlignCenter, "Nessuna misura valida");
      return;
    }

    double minX = points.first().angle;
    double maxX = minX;
    double maxY = 0.001;
    for (const Point& point : points)
    {
      minX = std::min(minX, point.angle);
      maxX = std::max(maxX, point.angle);
      maxY = std::max(maxY, point.error);
    }
    if (std::abs(maxX - minX) < 0.001)
    {
      maxX = minX + 1.0;
    }

    auto mapPoint = [&](double x, double y) {
      return QPointF(
        area.left() + (x - minX) / (maxX - minX) * area.width(),
        area.bottom() - y / maxY * area.height());
    };

    const QVector<QColor> colors = {
      QColor("#4da3ff"), QColor("#40c980"), QColor("#ffad42"),
      QColor("#d576ff"), QColor("#ff667a")
    };
    QMap<QString, int> colorById;
    int colorIndex = 0;
    QMap<int, QVector<Point>> byPass;
    for (const Point& point : points)
    {
      if (!colorById.contains(point.id))
      {
        colorById.insert(point.id, colorIndex++);
      }
      byPass[point.pass].append(point);
    }

    for (auto passIt = byPass.cbegin(); passIt != byPass.cend(); ++passIt)
    {
      QMap<QString, QVector<Point>> grouped;
      for (const Point& point : passIt.value())
      {
        grouped[point.id].append(point);
      }
      for (auto groupIt = grouped.cbegin(); groupIt != grouped.cend(); ++groupIt)
      {
        const QColor color = colors[colorById.value(groupIt.key()) % colors.size()];
        painter.setPen(QPen(color, 2));
        QPointF previous;
        bool hasPrevious = false;
        QVector<Point> sorted = groupIt.value();
        std::sort(sorted.begin(), sorted.end(), [](const Point& a, const Point& b) {
          return a.angle < b.angle;
        });
        for (const Point& point : sorted)
        {
          const QPointF current = mapPoint(point.angle, point.error);
          if (hasPrevious)
          {
            painter.drawLine(previous, current);
          }
          painter.setBrush(color);
          painter.drawEllipse(current, 4, 4);
          previous = current;
          hasPrevious = true;
        }
      }
    }

    painter.setPen(QColor("#aab4be"));
    painter.drawText(8, area.top() + 12, QString::number(maxY, 'f', 3));
    painter.drawText(8, area.bottom(), "0");
    painter.drawText(area.left(), height() - 12, QString::number(minX, 'f', 1) + "°");
    painter.drawText(area.right() - 45, height() - 12, QString::number(maxX, 'f', 1) + "°");
  }

private:
  QVector<TestResult> m_results;
};
