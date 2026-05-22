#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowConstructedGeometryModule : public MainWindowModuleBase
{
public:
  explicit MainWindowConstructedGeometryModule(MainWindowContext& context);

  void showConstructedGeometryPanel(const CameraConfig& camera);

private:
  void createLineLineIntersection(const CameraConfig& camera, const QString& firstLineId, const QString& secondLineId);
  void createLineCircleIntersection(const CameraConfig& camera, const QString& lineId, const QString& circleId);
  void createCircleCircleIntersection(const CameraConfig& camera, const QString& firstCircleId, const QString& secondCircleId);
};
