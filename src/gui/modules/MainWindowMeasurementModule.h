#pragma once

#include "gui/modules/MainWindowModuleBase.h"

class CameraConfig;

class MainWindowMeasurementModule : public MainWindowModuleBase
{
public:
  explicit MainWindowMeasurementModule(MainWindowContext& context);

  void showMeasurementPanel(const CameraConfig& camera);
  void showPointPointDistancePanel(const CameraConfig& camera);
  void showPointLineDistancePanel(const CameraConfig& camera);
  void showLineLineDistancePanel(const CameraConfig& camera);
  void showCircleDiameterPanel(const CameraConfig& camera);
  void showLineLineAnglePanel(const CameraConfig& camera);
  void rebuildMeasurementRecipe(const CameraConfig& camera);

private:
  void refreshMeasurementSources(const CameraConfig& camera);
  void createPointPointDistance(const CameraConfig& camera, const QString& pointAId, const QString& pointBId);
  void createPointLineDistance(const CameraConfig& camera, const QString& pointId, const QString& lineId);
  void createLineLineDistance(const CameraConfig& camera, const QString& lineAId, const QString& lineBId);
  void createCircleDiameter(const CameraConfig& camera, const QString& circleId);
  void createLineLineAngle(const CameraConfig& camera, const QString& lineAId, const QString& lineBId);
  void saveMeasurementRecipeAction(const CameraConfig& camera,
                                   const QString& type,
                                   const QString& sourceAId,
                                   const QString& sourceBId = {});
};
