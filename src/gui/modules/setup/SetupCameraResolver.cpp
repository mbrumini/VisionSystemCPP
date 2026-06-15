#include "gui/modules/setup/SetupCameraResolver.h"

CameraConfig currentConfiguredCamera(const AppConfig& config, const CameraConfig& fallback)
{
  for (const CameraConfig& camera : config.cameras())
  {
    if (camera.id == fallback.id)
    {
      return camera;
    }
  }

  return fallback;
}
