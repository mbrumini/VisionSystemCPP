#pragma once

#include "measurement/RealMeasurementTypes.h"

class MeasurementToleranceEvaluator
{
public:
  MeasurementJudgement evaluate(const RealMeasurementResult& result, const MeasurementTolerance& tolerance) const;
};
