#pragma once

#include <QString>

enum class MeasurementUnit
{
  Pixel,
  Millimeter,
  Degree
};

enum class MeasurementJudgement
{
  NotEvaluated,
  Ok,
  Nok
};

struct MeasurementTolerance
{
  QString id;
  double nominal = 0.0;
  double min = 0.0;
  double max = 0.0;
  MeasurementUnit unit = MeasurementUnit::Millimeter;
  bool enabled = false;
};

struct RealMeasurementResult
{
  QString id;
  QString cameraId;
  QString sourceAId;
  QString sourceBId;
  double valuePixels = 0.0;
  double valueMm = 0.0;
  double valueDegrees = 0.0;
  MeasurementUnit displayUnit = MeasurementUnit::Millimeter;
  MeasurementJudgement judgement = MeasurementJudgement::NotEvaluated;
};
