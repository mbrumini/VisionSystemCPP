#pragma once

#include "calibration/CalibrationTypes.h"

#include <QString>

class CalibrationRecipe
{
public:
  CameraCalibrationModel load(const QString& cameraRecipePath) const;
  bool save(const QString& cameraRecipePath, const CameraCalibrationModel& model, QString* errorMessage = nullptr) const;
};
