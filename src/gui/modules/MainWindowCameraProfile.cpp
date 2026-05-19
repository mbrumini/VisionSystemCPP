#include "gui/modules/MainWindowCameraProfile.h"

namespace MainWindowCameraProfile
{
bool isBwDimensional(const CameraConfig& camera, const AppConfig& /*config*/)
{
  return camera.profile.imageMode == "bw" && camera.profile.inspectionTypes.contains("dimensional");
}

bool isGrayscaleLocalization(const CameraConfig& camera, const AppConfig& /*config*/)
{
  const bool hasAi = camera.profile.inspectionTypes.contains("ai");
  const bool hasGrayscaleWork = camera.profile.inspectionTypes.contains("measurement") ||
    camera.profile.inspectionTypes.contains("surface");

  return camera.profile.imageMode == "grayscale" && (!hasAi || hasGrayscaleWork);
}
}
