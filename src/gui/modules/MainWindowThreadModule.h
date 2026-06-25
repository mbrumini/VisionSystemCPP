#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowThreadModule : public MainWindowModuleBase
{
public:
  explicit MainWindowThreadModule(MainWindowContext& context);

  void showThreadInspectionPanel(const CameraConfig& camera);
  void activateThreadExtractionRoiDrawing(const CameraConfig& camera);
  void clearThreadExtractionRoi(const CameraConfig& camera);
  void syncExtractionRoiOverlay(const CameraConfig& camera);
  void testThreadExtraction(const CameraConfig& camera);
};
