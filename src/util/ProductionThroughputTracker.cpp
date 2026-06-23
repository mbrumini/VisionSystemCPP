#include "util/ProductionThroughputTracker.h"

#include <algorithm>

void ProductionThroughputTracker::reset()
{
  m_timestamps.clear();
}

void ProductionThroughputTracker::recordPiece(qint64 timestampMs)
{
  m_timestamps.append(timestampMs);
  pruneOld(timestampMs, kAverageWindowMs + 5000);
}

bool ProductionThroughputTracker::hasData() const
{
  return !m_timestamps.isEmpty();
}

void ProductionThroughputTracker::pruneOld(qint64 nowMs, qint64 keepMs)
{
  const qint64 cutoff = nowMs - keepMs;
  while (!m_timestamps.isEmpty() && m_timestamps.first() < cutoff)
  {
    m_timestamps.removeFirst();
  }
}

double ProductionThroughputTracker::instantaneousPiecesPerMinute(qint64 nowMs) const
{
  Q_UNUSED(nowMs);
  if (m_timestamps.size() < 2)
  {
    return 0.0;
  }

  const qint64 deltaMs = m_timestamps.last() - m_timestamps[m_timestamps.size() - 2];
  if (deltaMs <= 0)
  {
    return 0.0;
  }

  return 60000.0 / static_cast<double>(deltaMs);
}

double ProductionThroughputTracker::averagePiecesPerMinute(qint64 nowMs, qint64 windowMs) const
{
  if (m_timestamps.isEmpty() || windowMs <= 0)
  {
    return 0.0;
  }

  const qint64 cutoff = nowMs - windowMs;
  int count = 0;
  qint64 oldestInWindow = nowMs;
  for (qint64 timestamp : m_timestamps)
  {
    if (timestamp < cutoff)
    {
      continue;
    }

    ++count;
    oldestInWindow = std::min(oldestInWindow, timestamp);
  }

  if (count <= 0)
  {
    return 0.0;
  }

  const qint64 spanMs = std::max<qint64>(1, nowMs - oldestInWindow);
  const qint64 effectiveSpanMs = std::min(windowMs, spanMs);
  return static_cast<double>(count) * 60000.0 / static_cast<double>(effectiveSpanMs);
}
