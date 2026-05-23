#pragma once

#include "calibration/ImageCalibrationMap.h"
#include "measurement/MeasurementGeometry.h"
#include "measurement/RealMeasurementTypes.h"

class MeasurementUnitConverter
{
public:
  RealMeasurementResult convert(const QString& cameraId,
                                const MeasurementResult& measurement,
                                const ImageCalibrationMap& calibration) const;
};
