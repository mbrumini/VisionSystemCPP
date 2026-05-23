#pragma once

#include "config/AppConfig.h"
#include "config/RecipeManager.h"
#include "runtime/CameraRuntime.h"

#include <QString>

struct MainWindowContext;

QString setupResultsText(const CameraConfig& camera,
                         const CameraRuntime* runtime,
                         const RecipeManager& recipes,
                         const MainWindowContext& context);
