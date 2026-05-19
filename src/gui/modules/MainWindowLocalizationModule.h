#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowLocalizationModule : public MainWindowModuleBase
{
public:
  explicit MainWindowLocalizationModule(MainWindowContext& context);

  void testLocalization(const CameraConfig& camera);
};
