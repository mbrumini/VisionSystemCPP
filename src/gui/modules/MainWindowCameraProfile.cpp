#include "gui/modules/MainWindowCameraProfile.h"

namespace MainWindowCameraProfile
{
bool isBwDimensional(const CameraConfig& camera, const AppConfig& /*config*/)
{
  return camera.profile.imageMode == "bw" && camera.profile.inspectionTypes.contains("dimensional");
}

bool isGrayscaleLocalization(const CameraConfig& camera, const AppConfig& /*config*/)
{
  return camera.exists && camera.enabled;
}
}
