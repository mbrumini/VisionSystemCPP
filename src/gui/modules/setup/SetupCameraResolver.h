#pragma once

#include "config/AppConfig.h"

CameraConfig currentConfiguredCamera(const AppConfig& config, const CameraConfig& fallback);
