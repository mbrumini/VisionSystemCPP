#include "measurement/MeasurementToleranceEvaluator.h"

MeasurementJudgement MeasurementToleranceEvaluator::evaluate(const RealMeasurementResult& result, const MeasurementTolerance& tolerance) const
{
  Q_UNUSED(result);
  Q_UNUSED(tolerance);
  return MeasurementJudgement::NotEvaluated;
}
