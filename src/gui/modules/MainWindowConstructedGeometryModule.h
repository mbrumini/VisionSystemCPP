#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowConstructedGeometryModule : public MainWindowModuleBase
{
public:
  explicit MainWindowConstructedGeometryModule(MainWindowContext& context);

  void showConstructedGeometryPanel(const CameraConfig& camera);
  void rebuildConstructedGeometryRecipe(const CameraConfig& camera);

private:
  void saveConstructedGeometryRecipeAction(const CameraConfig& camera,
                                           const QString& type,
                                           const QString& sourceAId,
                                           const QString& sourceBId = {},
                                           double offset = 0.0);

  void createLineLineIntersection(const CameraConfig& camera, const QString& firstLineId, const QString& secondLineId);
  void createLineCircleIntersection(const CameraConfig& camera, const QString& lineId, const QString& circleId);
  void createCircleCircleIntersection(const CameraConfig& camera, const QString& firstCircleId, const QString& secondCircleId);
  void createMidpoint(const CameraConfig& camera, const QString& firstPointId, const QString& secondPointId);
  void createOffsetLine(const CameraConfig& camera, const QString& lineId, double offset);
  void createAngleBisectors(const CameraConfig& camera, const QString& firstLineId, const QString& secondLineId);
  void createTangentLines(const CameraConfig& camera, const QString& pointId, const QString& circleId);
  void createProjectedPoint(const CameraConfig& camera, const QString& pointId, const QString& lineId);
};
