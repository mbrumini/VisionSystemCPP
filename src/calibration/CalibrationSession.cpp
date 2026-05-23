#include "calibration/CalibrationSession.h"

void CalibrationSession::setCameraId(const QString& cameraId)
{
  m_cameraId = cameraId;
}

void CalibrationSession::setPattern(const CalibrationPatternConfig& pattern)
{
  m_pattern = pattern;
}

void CalibrationSession::addFrame(const CalibrationFrame& frame)
{
  m_frames.append(frame);
}

void CalibrationSession::clearFrames()
{
  m_frames.clear();
}

const QString& CalibrationSession::cameraId() const
{
  return m_cameraId;
}

const CalibrationPatternConfig& CalibrationSession::pattern() const
{
  return m_pattern;
}

const QVector<CalibrationFrame>& CalibrationSession::frames() const
{
  return m_frames;
}
