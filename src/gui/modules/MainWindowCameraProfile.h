#pragma once

#include "config/AppConfig.h"

namespace MainWindowCameraProfile
{
bool isBwDimensional(const CameraConfig& camera, const AppConfig& config);
bool isGrayscaleLocalization(const CameraConfig& camera, const AppConfig& config);
}
