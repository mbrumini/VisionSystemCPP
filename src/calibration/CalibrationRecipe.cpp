#include "calibration/CalibrationRecipe.h"

CameraCalibrationModel CalibrationRecipe::load(const QString& cameraRecipePath) const
{
  Q_UNUSED(cameraRecipePath);
  return {};
}

bool CalibrationRecipe::save(const QString& cameraRecipePath, const CameraCalibrationModel& model, QString* errorMessage) const
{
  Q_UNUSED(cameraRecipePath);
  Q_UNUSED(model);
  Q_UNUSED(errorMessage);
  return false;
}
