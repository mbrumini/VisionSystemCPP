#pragma once

#include "calibration/CalibrationTypes.h"

class CalibrationSession
{
public:
  void setCameraId(const QString& cameraId);
  void setPattern(const CalibrationPatternConfig& pattern);
  void addFrame(const CalibrationFrame& frame);
  void clearFrames();

  const QString& cameraId() const;
  const CalibrationPatternConfig& pattern() const;
  const QVector<CalibrationFrame>& frames() const;

private:
  QString m_cameraId;
  CalibrationPatternConfig m_pattern;
  QVector<CalibrationFrame> m_frames;
};
