#pragma once

#include "config/AppConfig.h"
#include "gui/modules/MainWindowContext.h"
#include "gui/modules/MainWindowSetupModule.h"

#include <QString>

namespace GeometryPanelNavigation
{
inline bool shouldReturnToSetup(const MainWindowContext& context, const CameraConfig& camera)
{
  return context.returnToSetupCameraId && *context.returnToSetupCameraId == camera.id && context.setup;
}

inline QString backLabel(const MainWindowContext& context, const CameraConfig& camera, const QString& fallback)
{
  return shouldReturnToSetup(context, camera)
    ? context.trText("commands.backToSetup")
    : fallback;
}

inline bool returnToSetup(MainWindowContext& context, const CameraConfig& camera)
{
  if (!shouldReturnToSetup(context, camera))
  {
    return false;
  }

  context.returnToSetupCameraId->clear();
  context.setup->showCameraSetupPanel(camera);
  return true;
}
}
