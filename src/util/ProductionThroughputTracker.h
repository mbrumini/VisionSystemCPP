#pragma once

#include <QtGlobal>
#include <QVector>

class ProductionThroughputTracker
{
public:
  static constexpr qint64 kAverageWindowMs = 10000;

  void reset();
  void recordPiece(qint64 timestampMs);
  double instantaneousPiecesPerMinute(qint64 nowMs) const;
  double averagePiecesPerMinute(qint64 nowMs, qint64 windowMs = kAverageWindowMs) const;
  bool hasData() const;

private:
  void pruneOld(qint64 nowMs, qint64 keepMs);

  QVector<qint64> m_timestamps;
};
