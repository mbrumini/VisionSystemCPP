#include "measurement/MeasurementUnitConverter.h"

RealMeasurementResult MeasurementUnitConverter::convert(const QString& cameraId,
                                                        const MeasurementResult& measurement,
                                                        const ImageCalibrationMap& calibration) const
{
  Q_UNUSED(calibration);
  RealMeasurementResult result;
  result.id = measurement.id;
  result.cameraId = cameraId;
  result.sourceAId = measurement.sourceAId;
  result.sourceBId = measurement.sourceBId;
  result.valuePixels = measurement.valuePixels;
  result.valueDegrees = measurement.type == "line_line_angle" ? measurement.valuePixels : 0.0;
  result.displayUnit = measurement.type == "line_line_angle" ? MeasurementUnit::Degree : MeasurementUnit::Millimeter;
  return result;
}
